#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "opt.h"
#include "asm.h"

#include "platform.h" /* HACK. We need some platform specific code gen behavior. *sigh.* */


/* takes a list of nodes, and reduces it (and it's subnodes) to a list
 * following these constraints:
 *      - All nodes are expression nodes
 *      - Nodes with side effects are root nodes
 *      - All nodes operate on machine-primitive types and tuples
 */
typedef struct Simp Simp;
struct Simp {
    int isglobl;

    Node **stmts;
    size_t nstmts;

    /* return handling */
    Node *endlbl;
    Node *ret;
    int   isbigret;

    /* pre/postinc handling */
    Node **incqueue;
    size_t nqueue;
    
    /* break/continue handling */
    Node **loopstep;
    size_t nloopstep;
    Node **loopexit;
    size_t nloopexit;

    /* location handling */
    Node **blobs;
    size_t nblobs;
    size_t stksz;
    size_t argsz;
    Htab *globls;
    Htab *stkoff;
};

static char *asmname(Node *n);
static Node *simp(Simp *s, Node *n);
static Node *rval(Simp *s, Node *n, Node *dst);
static Node *lval(Simp *s, Node *n);
static Node *assign(Simp *s, Node *lhs, Node *rhs);
static void declarelocal(Simp *s, Node *n);
static void simpcond(Simp *s, Node *n, Node *ltrue, Node *lfalse);
static void simpconstinit(Simp *s, Node *dcl);
static Node *simpcast(Simp *s, Node *val, Type *to);
static Node *simpslice(Simp *s, Node *n, Node *dst);
static Node *idxaddr(Simp *s, Node *seq, Node *idx);
static void umatch(Simp *s, Node *pat, Node *val, Type *t, Node *iftrue, Node *iffalse);

/* useful constants */
static Type *tyintptr;
static Type *tyword;
static Type *tyvoid;

size_t tyalign(size_t sz, size_t eltsz)
{
    return align(sz, min(eltsz, Ptrsz));
}

static Type *base(Type *t)
{
    assert(t->nsub == 1);
    return t->sub[0];
}

static Node *add(Node *a, Node *b)
{
    Node *n;

    assert(size(a) == size(b));
    n = mkexpr(a->line, Oadd, a, b, NULL);
    n->expr.type = a->expr.type;
    return n;
}

static Node *addk(Node *n, uvlong v)
{
    Node *k;

    k = mkintlit(n->line, v);
    k->expr.type = exprtype(n);
    return add(n, k);
}

static Node *sub(Node *a, Node *b)
{
    Node *n;

    n = mkexpr(a->line, Osub, a, b, NULL);
    n->expr.type = a->expr.type;
    return n;
}

static Node *subk(Node *n, uvlong v)
{
    Node *k;

    k = mkintlit(n->line, v);
    k->expr.type = exprtype(n);
    return sub(n, k);
}

static Node *mul(Node *a, Node *b)
{
    Node *n;

    n = mkexpr(a->line, Omul, a, b, NULL);
    n->expr.type = a->expr.type;
    return n;
}

static int addressable(Simp *s, Node *a)
{
    if (a->type == Ndecl || (a->type == Nexpr && exprop(a) == Ovar))
        return hthas(s->stkoff, a) || hthas(s->globls, a);
    else
        return stacknode(a);
}

/* takes the address of a node, possibly converting it to
 * a pointer to the base type 'bt' */
static Node *addr(Simp *s, Node *a, Type *bt)
{
    Node *n;

    n = mkexpr(a->line, Oaddr, a, NULL);
    if (!addressable(s, a))
            declarelocal(s, a);
    if (!bt)
        n->expr.type = mktyptr(a->line, a->expr.type);
    else
        n->expr.type = mktyptr(a->line, bt);
    return n;
}

static Node *load(Node *a)
{
    Node *n;

    assert(a->expr.type->type == Typtr);
    n = mkexpr(a->line, Oderef, a, NULL);
    n->expr.type = base(a->expr.type);
    return n;
}

static Node *deref(Node *a)
{
    Node *n;

    assert(a->expr.type->type == Typtr);
    n = mkexpr(a->line, Oderef, a, NULL);
    n->expr.type = base(a->expr.type);
    return n;
}

static Node *set(Node *a, Node *b)
{
    Node *n;

    assert(a != NULL && b != NULL);
    assert(exprop(a) == Ovar || exprop(a) == Oderef);
    n = mkexpr(a->line, Oset, a, b, NULL);
    n->expr.type = exprtype(a);
    return n;
}

static Node *disp(int line, uint v)
{
    Node *n;

    n = mkintlit(line, v);
    n->expr.type = tyintptr;
    return n;
}

static Node *word(int line, uint v)
{
    Node *n;

    n = mkintlit(line, v);
    n->expr.type = tyword;
    return n;
}

static void append(Simp *s, Node *n)
{
    lappend(&s->stmts, &s->nstmts, n);
}

static int ispure(Node *n)
{
    return ispureop[exprop(n)];
}

static int isconstfn(Node *s)
{
    return s->decl.isconst && decltype(s)->type == Tyfunc;
}

/* For x86, the assembly names are generated as follows:
 *      local symbols: .name
 *      un-namespaced symbols: <symprefix>name
 *      namespaced symbols: <symprefix>namespace$name
 */
static char *asmname(Node *n)
{
    char *s;
    int len;

    len = strlen(Symprefix);
    if (n->name.ns)
        len += strlen(n->name.ns) + 1; /* +1 for separator */
    len += strlen(n->name.name) + 1;

    s = xalloc(len + 1);
    s[0] = '\0';
    if (n->name.ns)
        snprintf(s, len, "%s%s$%s", Symprefix, n->name.ns, n->name.name);
    else if (n->name.name[0] == '.')
        snprintf(s, len, "%s", n->name.name);
    else
        snprintf(s, len, "%s%s", Symprefix, n->name.name);
    return s;
}

int stacktype(Type *t)
{
    /* the types are arranged in types.def such that this is true */
    t = tybase(t);
    return t->type >= Tyslice;
}

int floattype(Type *t)
{
    t = tybase(t);
    return t->type == Tyfloat32 || t->type == Tyfloat64;
}

int stacknode(Node *n)
{
    if (n->type == Nexpr)
        return stacktype(n->expr.type);
    else
        return stacktype(n->decl.type);
}

int floatnode(Node *n)
{
    if (n->type == Nexpr)
        return floattype(n->expr.type);
    else
        return floattype(n->decl.type);
}

size_t tysize(Type *t)
{
    size_t sz;
    size_t i;

    sz = 0;
    if (!t)
        die("size of empty type => bailing.");
    switch (t->type) {
        case Tyvoid:
            die("void has no size");
            return 1;
        case Tybool: case Tyint8:
        case Tybyte: case Tyuint8:
            return 1;
        case Tyint16: case Tyuint16:
            return 2;
        case Tyint: case Tyint32:
        case Tyuint: case Tyuint32:
        case Tychar:  /* utf32 */
            return 4;

        case Typtr: case Tyfunc:
        case Tyvalist: /* ptr to first element of valist */
            return Ptrsz;

        case Tyint64: case Tylong:
        case Tyuint64: case Tyulong:
            return 8;

            /*end integer types*/
        case Tyfloat32:
            return 4;
        case Tyfloat64:
            return 8;

        case Tyslice:
            return 2*Ptrsz; /* len; ptr */
        case Tyname:
            return tysize(t->sub[0]);
        case Tyarray:
            t->asize = fold(t->asize, 1);
            assert(exprop(t->asize) == Olit);
            return t->asize->expr.args[0]->lit.intval * tysize(t->sub[0]);
        case Tytuple:
            for (i = 0; i < t->nsub; i++) {
                sz = tyalign(sz, tysize(t->sub[i]));
                sz += tysize(t->sub[i]);
            }
            for (i = 0; i < t->nsub; i++)
                sz = tyalign(sz, tysize(t->sub[i]));
            return sz;
            break;
        case Tystruct:
            for (i = 0; i < t->nmemb; i++) {
                sz = tyalign(sz, size(t->sdecls[i]));
                sz += size(t->sdecls[i]);
            }
            /* the whole struct size should match the biggest alignment */
            for (i = 0; i < t->nmemb; i++)
                sz = tyalign(sz, size(t->sdecls[i]));
            return sz;
            break;
        case Tyunion:
            sz = Wordsz;
            for (i = 0; i < t->nmemb; i++)
                if (t->udecls[i]->etype)
                    sz = max(sz, tysize(t->udecls[i]->etype) + Wordsz);
            return align(sz, Ptrsz);
            break;
        case Tybad: case Tyvar: case Typaram: case Tyunres: case Ntypes:
            die("Type %s does not have size; why did it get down to here?", tystr(t));
            break;
    }
    return -1;
}

size_t size(Node *n)
{
    Type *t;

    if (n->type == Nexpr)
        t = n->expr.type;
    else
        t = n->decl.type;
    return tysize(t);
}

static Node *gentemp(Simp *simp, Node *e, Type *ty, Node **dcl)
{
    char buf[128];
    static int nexttmp;
    Node *t, *r, *n;

    snprintf(buf, 128, ".t%d", nexttmp++);
    n = mkname(e->line, buf);
    t = mkdecl(e->line, n, ty);
    r = mkexpr(e->line, Ovar, n, NULL);
    r->expr.type = t->decl.type;
    r->expr.did = t->decl.did;
    if (dcl)
        *dcl = t;
    return r;
}

static Node *temp(Simp *simp, Node *e)
{
    Node *t, *dcl;

    assert(e->type == Nexpr);
    t = gentemp(simp, e, e->expr.type, &dcl);
    if (stacknode(e))
        declarelocal(simp, dcl);
    return t;
}

static void jmp(Simp *s, Node *lbl)
{
    append(s, mkexpr(lbl->line, Ojmp, lbl, NULL));
}

static void cjmp(Simp *s, Node *cond, Node *iftrue, Node *iffalse)
{
    Node *jmp;

    jmp = mkexpr(cond->line, Ocjmp, cond, iftrue, iffalse, NULL);
    append(s, jmp);
}

static Node *slicelen(Simp *s, Node *sl)
{
    /* *(&sl + sizeof(size_t)) */
    return load(addk(addr(s, sl, tyintptr), Ptrsz));
}


static Node *seqlen(Simp *s, Node *n, Type *ty)
{
    Node *t, *r;

    if (exprtype(n)->type == Tyslice) {
        t = slicelen(s, n);
        r = simpcast(s, t, ty);
    } else if (exprtype(n)->type == Tyarray) {
        t = exprtype(n)->asize;
        r = simpcast(s, t, ty);
    } else {
        r = NULL;
    }
    return r;
}

/* if foo; bar; else baz;;
 *      => cjmp (foo) :bar :baz */
static void simpif(Simp *s, Node *n, Node *exit)
{
    Node *l1, *l2, *l3;
    Node *iftrue, *iffalse;

    l1 = genlbl();
    l2 = genlbl();
    if (exit)
        l3 = exit;
    else
        l3 = genlbl();

    iftrue = n->ifstmt.iftrue;
    iffalse = n->ifstmt.iffalse;

    simpcond(s, n->ifstmt.cond, l1, l2);
    simp(s, l1);
    simp(s, iftrue);
    jmp(s, l3);
    simp(s, l2);
    /* because lots of bunched up end labels are ugly,
     * coalesce them by handling 'elif'-like constructs
     * separately */
    if (iffalse && iffalse->type == Nifstmt) {
        simpif(s, iffalse, exit);
    } else {
        simp(s, iffalse);
        jmp(s, l3);
    }

    if (!exit)
        simp(s, l3);
}

/* init; while cond; body;; 
 *    => init
 *       jmp :cond
 *       :body
 *           ...body...
 *           ...step...
 *       :cond
 *           ...cond...
 *            cjmp (cond) :body :end
 *       :end
 */
static void simploop(Simp *s, Node *n)
{
    Node *lbody;
    Node *lend;
    Node *lcond;
    Node *lstep;

    lbody = genlbl();
    lcond = genlbl();
    lstep = genlbl();
    lend = genlbl();

    lappend(&s->loopstep, &s->nloopstep, lstep);
    lappend(&s->loopexit, &s->nloopexit, lend);

    simp(s, n->loopstmt.init);  /* init */
    jmp(s, lcond);              /* goto test */
    simp(s, lbody);             /* body lbl */
    simp(s, n->loopstmt.body);  /* body */
    simp(s, lstep);             /* test lbl */
    simp(s, n->loopstmt.step);  /* step */
    simp(s, lcond);             /* test lbl */
    simpcond(s, n->loopstmt.cond, lbody, lend);    /* repeat? */
    simp(s, lend);              /* exit */

    s->nloopstep--;
    s->nloopexit--;
}

/* pat; seq; 
 *      body;;
 *
 * =>
 *      .pseudo = seqinit
 *      jmp :cond
 *      :body
 *           ...body...
 *      :step
 *           ...step...
 *      :cond
 *           ...cond...
 *           cjmp (cond) :match :end
 *      :match
 *           ...match...
 *           cjmp (match) :body :step
 *      :end
 */
static void simpiter(Simp *s, Node *n)
{
    Node *lbody, *lstep, *lcond, *lmatch, *lend;
    Node *idx, *len, *dcl, *seq, *val, *done;
    Node *zero;

    lbody = genlbl();
    lstep = genlbl();
    lcond = genlbl();
    lmatch = genlbl();
    lend = genlbl();

    lappend(&s->loopstep, &s->nloopstep, lstep);
    lappend(&s->loopexit, &s->nloopexit, lend);

    zero = mkintlit(n->line, 0);
    zero->expr.type = tyintptr;

    seq = rval(s, n->iterstmt.seq, NULL);
    idx = gentemp(s, n, tyintptr, &dcl);
    declarelocal(s, dcl);

    /* setup */
    append(s, assign(s, idx, zero));
    jmp(s, lcond);
    simp(s, lbody);
    /* body */
    simp(s, n->iterstmt.body);
    /* step */
    simp(s, lstep);
    simp(s, assign(s, idx, addk(idx, 1)));
    /* condition */
    simp(s, lcond);
    len = seqlen(s, seq, tyintptr);
    done = mkexpr(n->line, Olt, idx, len, NULL);
    cjmp(s, done, lmatch, lend);
    simp(s, lmatch);
    val = load(idxaddr(s, seq, idx));
    umatch(s, n->iterstmt.elt, val, val->expr.type, lbody, lstep);
    simp(s, lend);

    s->nloopstep--;
    s->nloopexit--;
}

static Ucon *finducon(Node *n)
{
    size_t i;
    Type *t;
    Ucon *uc;

    t = tybase(n->expr.type);
    if (exprop(n) != Oucon)
        return NULL;
    for (i = 0; i  < t->nmemb; i++) {
        uc = t->udecls[i];
        if (!strcmp(namestr(uc->name), namestr(n->expr.args[0])))
            return uc;
    }
    die("No ucon?!?");
    return NULL;
}

static Node *uconid(Simp *s, Node *n)
{
    Ucon *uc;

    if (exprop(n) != Oucon)
        return load(addr(s, n, mktype(n->line, Tyuint)));

    uc = finducon(n);
    return word(uc->line, uc->id);
}

static Node *patval(Simp *s, Node *n, Type *t)
{
    if (exprop(n) == Oucon)
        return n->expr.args[1];
    else if (exprop(n) == Olit)
        return n;
    else
        return load(addk(addr(s, n, t), Wordsz));
}

static void umatch(Simp *s, Node *pat, Node *val, Type *t, Node *iftrue, Node *iffalse)
{
    Node *v, *x, *y;
    Node *deeper, *next;
    Node **patarg;
    Ucon *uc;
    size_t i;
    size_t off;

    assert(pat->type == Nexpr);
    t = tybase(t);
    if (exprop(pat) == Ovar && !decls[pat->expr.did]->decl.isconst) {
        v = assign(s, pat, val);
        append(s, v);
        jmp(s, iftrue);
        return;
    }
    switch (t->type) {
        /* Never supported */
        case Tyvoid: case Tybad: case Tyvalist: case Tyvar:
        case Typaram: case Tyunres: case Tyname: case Ntypes:
        /* Should never show up */
        case Tyslice:
            die("Unsupported type for compare");
            break;
        case Tybool: case Tychar: case Tybyte:
        case Tyint8: case Tyint16: case Tyint32: case Tyint:
        case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint:
        case Tyint64: case Tyuint64: case Tylong:  case Tyulong:
        case Tyfloat32: case Tyfloat64:
        case Typtr: case Tyfunc:
            v = mkexpr(pat->line, Oeq, pat, val, NULL);
            v->expr.type = mktype(pat->line, Tybool);
            cjmp(s, v, iftrue, iffalse);
            break;
        /* We got lucky. The structure of tuple, array, and struct literals
         * is the same, so long as we don't inspect the type, so we can
         * share the code*/
        case Tystruct: case Tytuple: case Tyarray: 
            patarg = pat->expr.args;
            off = 0;
            for (i = 0; i < pat->expr.nargs; i++) {
                off = tyalign(off, size(patarg[i]));
                next = genlbl();
                v = load(addk(addr(s, val, exprtype(patarg[i])), off));
                umatch(s, patarg[i], v, exprtype(patarg[i]), next, iffalse);
                append(s, next);
                off += size(patarg[i]);
            }
            jmp(s, iftrue);
            break;
        case Tyunion:
            uc = finducon(pat);
            if (!uc)
                uc = finducon(val);

            deeper = genlbl();

            x = uconid(s, pat);
            y = uconid(s, val);
            v = mkexpr(pat->line, Oeq, x, y, NULL);
            v->expr.type = tyintptr;
            cjmp(s, v, deeper, iffalse);
            append(s, deeper);
            if (uc->etype) {
                pat = patval(s, pat, uc->etype);
                val = patval(s, val, uc->etype);
                umatch(s, pat, val, uc->etype, iftrue, iffalse);
            }
            break;
    }
}

static void simpmatch(Simp *s, Node *n)
{
    Node *end, *cur, *next; /* labels */
    Node *val, *tmp;
    Node *m;
    size_t i;

    end = genlbl();
    val = temp(s, n->matchstmt.val);
    tmp = rval(s, n->matchstmt.val, val);
    if (val != tmp)
        append(s, assign(s, val, tmp));
    for (i = 0; i < n->matchstmt.nmatches; i++) {
        m = n->matchstmt.matches[i];

        /* check pattern */
        cur = genlbl();
        next = genlbl();
        umatch(s, m->match.pat, val, val->expr.type, cur, next);

        /* do the action if it matches */
        append(s, cur);
        simp(s, m->match.block);
        jmp(s, end);
        append(s, next);
    }
    append(s, end);
}

static void simpblk(Simp *s, Node *n)
{
    size_t i;

    pushstab(n->block.scope);
    for (i = 0; i < n->block.nstmts; i++) {
        n->block.stmts[i] = fold(n->block.stmts[i], 0);
        simp(s, n->block.stmts[i]);
    }
    popstab();
}

static Node *simpblob(Simp *s, Node *blob, Node ***l, size_t *nl)
{
    Node *n, *d, *r;
    char lbl[128];

    n = mkname(blob->line, genlblstr(lbl, 128));
    d = mkdecl(blob->line, n, blob->expr.type);
    r = mkexpr(blob->line, Ovar, n, NULL);

    d->decl.init = blob;
    d->decl.type = blob->expr.type;
    d->decl.isconst = 1;
    htput(s->globls, d, strdup(lbl));

    r->expr.did = d->decl.did;
    r->expr.type = blob->expr.type;
    r->expr.isconst = 1;

    lappend(l, nl, d);
    return r;
}

/* gets the byte offset of 'memb' within the aggregate type 'aggr' */
static size_t offset(Node *aggr, Node *memb)
{
    Type *ty;
    Node **nl;
    size_t i;
    size_t off;

    ty = tybase(exprtype(aggr));
    if (ty->type == Typtr)
        ty = tybase(ty->sub[0]);

    assert(ty->type == Tystruct);
    nl = ty->sdecls;
    off = 0;
    for (i = 0; i < ty->nmemb; i++) {
        off = tyalign(off, size(nl[i]));
        if (!strcmp(namestr(memb), declname(nl[i])))
            return off;
        off += size(nl[i]);
    }
    die("Could not find member %s in struct", namestr(memb));
    return -1;
}

static Node *ptrsized(Simp *s, Node *v)
{
    if (size(v) == Ptrsz)
        return v;
    else if (size(v) < Ptrsz)
        v = mkexpr(v->line, Ozwiden, v, NULL);
    else if (size(v) > Ptrsz)
        v = mkexpr(v->line, Otrunc, v, NULL);
    v->expr.type = tyintptr;
    return v;
}

static Node *membaddr(Simp *s, Node *n)
{
    Node *t, *u, *r;
    Node **args;
    Type *ty;

    args = n->expr.args;
    ty = tybase(exprtype(args[0]));
    if (ty->type == Typtr) {
        t = lval(s, args[0]);
    } else {
        t = addr(s, lval(s, args[0]), exprtype(n));
    }
    u = disp(n->line, offset(args[0], args[1]));
    r = add(t, u);
    r->expr.type = mktyptr(n->line, n->expr.type);
    return r;
}

static Node *idxaddr(Simp *s, Node *seq, Node *idx)
{
    Node *a, *t, *u, *v; /* temps */
    Node *r; /* result */
    Type *ty;
    size_t sz;

    a = rval(s, seq, NULL);
    ty = exprtype(seq)->sub[0];
    if (exprtype(seq)->type == Tyarray)
        t = addr(s, a, ty);
    else if (seq->expr.type->type == Tyslice)
        t = load(addr(s, a, mktyptr(seq->line, ty)));
    else
        die("Can't index type %s\n", tystr(seq->expr.type));
    assert(t->expr.type->type == Typtr);
    u = rval(s, idx, NULL);
    u = ptrsized(s, u);
    sz = tysize(ty);
    v = mul(u, disp(seq->line, sz));
    r = add(t, v);
    return r;
}

static Node *slicebase(Simp *s, Node *n, Node *off)
{
    Node *t, *u, *v;
    Type *ty;
    int sz;

    t = rval(s, n, NULL);
    u = NULL;
    ty = tybase(exprtype(n));
    switch (ty->type) {
        case Typtr:     u = t; break;
        case Tyarray:   u = addr(s, t, base(exprtype(n))); break;
        case Tyslice:   u = load(addr(s, t, mktyptr(n->line, base(exprtype(n))))); break;
        default: die("Unslicable type %s", tystr(n->expr.type));
    }
    /* safe: all types we allow here have a sub[0] that we want to grab */
    if (off) {
      off = ptrsized(s, rval(s, off, NULL));
      sz = tysize(n->expr.type->sub[0]);
      v = mul(off, disp(n->line, sz));
      return add(u, v);
    } else {
      return u;
    }
}

static Node *lval(Simp *s, Node *n)
{
    Node *r;

    switch (exprop(n)) {
        case Ovar:      r = n;  break;
        case Oidx:      r = deref(idxaddr(s, n->expr.args[0], n->expr.args[1])); break;
        case Oderef:    r = deref(rval(s, n->expr.args[0], NULL)); break;
        case Omemb:     r = deref(membaddr(s, n)); break;
        default:
            die("%s cannot be an lval", opstr(exprop(n)));
            break;
    }
    return r;
}

static void simpcond(Simp *s, Node *n, Node *ltrue, Node *lfalse)
{
    Node **args;
    Node *v, *lnext;

    args = n->expr.args;
    switch (exprop(n)) {
        case Oland:
            lnext = genlbl();
            simpcond(s, args[0], lnext, lfalse);
            append(s, lnext);
            simpcond(s, args[1], ltrue, lfalse);
            break;
        case Olor:
            lnext = genlbl();
            simpcond(s, args[0], ltrue, lnext);
            append(s, lnext);
            simpcond(s, args[1], ltrue, lfalse);
            break;
        case Olnot:
            simpcond(s, args[0], lfalse, ltrue);
            break;
        default:
            v = rval(s, n, NULL);
            cjmp(s, v, ltrue, lfalse);
            break;
    }
}

static Node *intconvert(Simp *s, Node *from, Type *to, int issigned)
{
    Node *r;
    size_t fromsz, tosz;

    fromsz = size(from);
    tosz = tysize(to);
    r = rval(s, from, NULL);
    if (fromsz > tosz) {
        r = mkexpr(from->line, Otrunc, r, NULL);
    } else if (tosz > fromsz) {
        if (issigned)
            r = mkexpr(from->line, Oswiden, r, NULL);
        else
            r = mkexpr(from->line, Ozwiden, r, NULL);
    }
    r->expr.type = to;
    return r;
}

static Node *simpcast(Simp *s, Node *val, Type *to)
{
    Node *r;
    Type *t;

    r = NULL;
    /* do the type conversion */
    switch (tybase(to)->type) {
        case Tybool:
        case Tyint8: case Tyint16: case Tyint32: case Tyint64:
        case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
        case Tyint: case Tyuint: case Tylong: case Tyulong:
        case Tychar: case Tybyte:
        case Typtr:
            t = tybase(exprtype(val));
            switch (t->type) {
                /* ptr -> slice conversion is disallowed */
                case Tyslice:
                    if (t->type == Typtr)
                        fatal(val->line, "Bad cast from %s to %s",
                              tystr(exprtype(val)), tystr(to));
                    r = slicebase(s, val, NULL);
                    break;
                /* signed conversions */
                case Tyint8: case Tyint16: case Tyint32: case Tyint64:
                case Tyint: case Tylong:
                    r = intconvert(s, val, to, 1);
                    break;
                /* unsigned conversions */
                case Tybool:
                case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
                case Tyuint: case Tyulong: case Tychar: case Tybyte:
                case Typtr:
                    r = intconvert(s, val, to, 0);
                    break;
                case Tyfloat32: case Tyfloat64:
                    if (tybase(to)->type == Typtr)
                        fatal(val->line, "Bad cast from %s to %s",
                              tystr(exprtype(val)), tystr(to));
                    r = mkexpr(val->line, Oflt2int, rval(s, val, NULL), NULL);
                    r->expr.type = to;
                    break;
                default:
                    fatal(val->line, "Bad cast from %s to %s",
                          tystr(exprtype(val)), tystr(to));
            }
            break;
        case Tyfloat32: case Tyfloat64:
            t = tybase(exprtype(val));
            switch (t->type) {
                case Tyint8: case Tyint16: case Tyint32: case Tyint64:
                case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
                case Tyint: case Tyuint: case Tylong: case Tyulong:
                case Tychar: case Tybyte:
                    r = mkexpr(val->line, Oflt2int, rval(s, val, NULL), NULL);
                    r->expr.type = to;
                    break;
                default:
                    fatal(val->line, "Bad cast from %s to %s",
                          tystr(exprtype(val)), tystr(to));
                    break;
            }
            break;
        /* no other destination types are handled as things stand */
        default:
            fatal(val->line, "Bad cast from %s to %s",
                  tystr(exprtype(val)), tystr(to));
    }
    return r;
}

/* Simplifies taking a slice of an array, pointer,
 * or other slice down to primitive pointer operations */
static Node *simpslice(Simp *s, Node *n, Node *dst)
{
    Node *t;
    Node *start, *end;
    Node *base, *sz, *len;
    Node *stbase, *stlen;

    if (dst)
        t = dst;
    else
        t = temp(s, n);
    /* *(&slice) = (void*)base + off*sz */
    base = slicebase(s, n->expr.args[0], n->expr.args[1]);
    start = ptrsized(s, rval(s, n->expr.args[1], NULL));
    end = ptrsized(s, rval(s, n->expr.args[2], NULL));
    len = sub(end, start);
    /* we can be storing through a pointer, in the case
     * of '*foo = bar'. */
    if (tybase(exprtype(t))->type == Typtr) {
        stbase = set(simpcast(s, t, mktyptr(t->line, tyintptr)), base);
        sz = addk(simpcast(s, t, mktyptr(t->line, tyintptr)), Ptrsz);
    } else {
        stbase = set(deref(addr(s, t, tyintptr)), base);
        sz = addk(addr(s, t, tyintptr), Ptrsz);
    }
    /* *(&slice + ptrsz) = len */
    stlen = set(deref(sz), len);
    append(s, stbase);
    append(s, stlen);
    return t;
}

static Node *visit(Simp *s, Node *n)
{
    size_t i;
    Node *r;

    for (i = 0; i < n->expr.nargs; i++)
        n->expr.args[i] = rval(s, n->expr.args[i], NULL);
    if (ispure(n)) {
        r = n;
    } else {
        if (exprtype(n)->type == Tyvoid) {
            r = NULL;
            append(s, n);
        } else {
            r = temp(s, n);
            append(s, set(r, n));
        }
    }
    return r;
}

/* Takes a tuple and binds the i'th element of it to the
 * i'th name on the rhs of the assignment. */
static Node *destructure(Simp *s, Node *lhs, Node *rhs)
{
    Node *plv, *prv, *lv, *sz, *stor, **args;
    size_t off, i;

    args = lhs->expr.args;
    rhs = rval(s, rhs, NULL);
    off = 0;
    for (i = 0; i < lhs->expr.nargs; i++) {
        lv = lval(s, args[i]);
        off = tyalign(off, size(lv));
        prv = add(addr(s, rhs, exprtype(args[i])), disp(rhs->line, off));
        if (stacknode(args[i])) {
            sz = disp(lhs->line, size(lv));
            plv = addr(s, lv, exprtype(lv));
            stor = mkexpr(lhs->line, Oblit, plv, prv, sz, NULL);
        } else {
            stor = set(lv, load(prv));
        }
        append(s, stor);
        off += size(lv);
    }

    return NULL;
}

static Node *assign(Simp *s, Node *lhs, Node *rhs)
{
    Node *t, *u, *v, *r;

    if (exprop(lhs) == Otup) {
        r = destructure(s, lhs, rhs);
    } else {
        t = lval(s, lhs);
        u = rval(s, rhs, t);

        /* if we stored the result into t, rval() should return that,
         * so we know our work is done. */
        if (u == t) {
            r = t;
        } else if (stacknode(lhs)) {
            t = addr(s, t, exprtype(lhs));
            u = addr(s, u, exprtype(lhs));
            v = disp(lhs->line, size(lhs));
            r = mkexpr(lhs->line, Oblit, t, u, v, NULL);
        } else {
            r = set(t, u);
        }
    }
    return r;
}

static Node *assignat(Simp *s, Node *r, size_t off, Node *val)
{
    Node *pval, *pdst;
    Node *sz;
    Node *st;

    val = rval(s, val, NULL);
    pdst = add(r, disp(val->line, off));

    if (stacknode(val)) {
        sz = disp(val->line, size(val));
        pval = addr(s, val, exprtype(val));
        st = mkexpr(val->line, Oblit, pdst, pval, sz, NULL);
    } else {
        st = set(deref(pdst), val);
    }
    append(s, st);
    return r;
}

/* Simplify tuple construction to a stack allocated
 * value by evaluating the rvalue of each node on the
 * rhs and assigning it to the correct offset from the
 * head of the tuple. */
static Node *simptup(Simp *s, Node *n, Node *dst)
{
    Node **args;
    Node *r;
    size_t i, off;

    args = n->expr.args;
    if (!dst)
        dst = temp(s, n);
    r = addr(s, dst, exprtype(dst));

    off = 0;
    for (i = 0; i < n->expr.nargs; i++) {
        off = tyalign(off, size(args[i]));
        assignat(s, r, off, args[i]);
        off += size(args[i]);
    }
    return dst;
}

static Node *simpucon(Simp *s, Node *n, Node *dst)
{
    Node *tmp, *u, *tag, *elt, *sz;
    Node *r;
    Type *ty;
    Ucon *uc;
    size_t i;

    /* find the ucon we're constructing here */
    ty = tybase(n->expr.type);
    uc = NULL;
    for (i = 0; i < ty->nmemb; i++) {
        if (!strcmp(namestr(n->expr.args[0]), namestr(ty->udecls[i]->name))) {
            uc = ty->udecls[i];
            break;
        }
    }
    if (!uc)
        die("Couldn't find union constructor");

    if (dst)
        tmp = dst;
    else
        tmp = temp(s, n);

    /* Set the tag on the ucon */
    u = addr(s, tmp, mktype(n->line, Tyuint));
    tag = mkintlit(n->line, uc->id);
    tag->expr.type = mktype(n->line, Tyuint);
    append(s, set(deref(u), tag));


    /* fill the value, if needed */
    if (!uc->etype)
        return tmp;
    elt = rval(s, n->expr.args[1], NULL);
    u = addk(u, Wordsz);
    if (stacktype(uc->etype)) {
        elt = addr(s, elt, uc->etype);
        sz = disp(n->line, tysize(uc->etype));
        r = mkexpr(n->line, Oblit, u, elt, sz, NULL);
    } else {
        r = set(deref(u), elt);
    }
    append(s, r);
    return tmp;
}

static Node *simpuget(Simp *s, Node *n, Node *dst)
{
    die("No uget simplification yet");
}

/* simplifies 
 *      a || b
 * to
 *      if a || b
 *              t = true
 *      else
 *              t = false
 *      ;;
 */
static Node *simplazy(Simp *s, Node *n)
{
    Node *r, *t, *u;
    Node *ltrue, *lfalse, *ldone;

    /* set up temps and labels */
    r = temp(s, n);
    ltrue = genlbl();
    lfalse = genlbl();
    ldone = genlbl();

    /* simp the conditional */
    simpcond(s, n, ltrue, lfalse);

    /* if true */
    append(s, ltrue);
    u = mkexpr(n->line, Olit, mkbool(n->line, 1), NULL);
    u->expr.type = mktype(n->line, Tybool);
    t = set(r, u);
    append(s, t);
    jmp(s, ldone);

    /* if false */
    append(s, lfalse);
    u = mkexpr(n->line, Olit, mkbool(n->line, 0), NULL);
    u->expr.type = mktype(n->line, Tybool);
    t = set(r, u);
    append(s, t);
    jmp(s, ldone);

    /* finish */
    append(s, ldone);
    return r;
}

static Node *rval(Simp *s, Node *n, Node *dst)
{
    Node *r; /* expression result */
    Node *t, *u, *v; /* temporary nodes */
    Node **args;
    size_t i;
    const Op fusedmap[Numops] = {
        [Oaddeq]        = Oadd,
        [Osubeq]        = Osub,
        [Omuleq]        = Omul,
        [Odiveq]        = Odiv,
        [Omodeq]        = Omod,
        [Oboreq]        = Obor,
        [Obandeq]       = Oband,
        [Obxoreq]       = Obxor,
        [Obsleq]        = Obsl,
        [Obsreq]        = Obsr,
    };
    const Op cmpmap[Numops][3] = {
        [Oeq]   = {Oeq, Ofeq, Oueq},
        [One]   = {One, Ofne, Oune},
        [Ogt]   = {Ogt, Ofgt, Ougt},
        [Oge]   = {Oge, Ofge, Ouge},
        [Olt]   = {Olt, Oflt, Oult},
        [Ole]   = {Ole, Ofle, Oule}
    };

    r = NULL;
    args = n->expr.args;
    switch (exprop(n)) {
        case Olor: case Oland:
            r = simplazy(s, n);
            break;
        case Osize:
            r = mkintlit(n->line, size(args[0]));
            r->expr.type = exprtype(n);
            break;
        case Oslice:
            r = simpslice(s, n, dst);
            break;
        case Oidx:
            t = idxaddr(s, n->expr.args[0], n->expr.args[1]);
            r = load(t);
            break;
        /* array.len slice.len are magic 'virtual' members.
         * they need to be special cased. */
        case Omemb:
            if (exprtype(args[0])->type == Tyslice || exprtype(args[0])->type == Tyarray) {
                r = seqlen(s, args[0], exprtype(n));
            } else {
                t = membaddr(s, n);
                r = load(t);
            }
            break;
        case Oucon:
            r = simpucon(s, n, dst);
            break;
        case Ouget:
            r = simpuget(s, n, dst);
            break;
        case Otup:
            r = simptup(s, n, dst);
            break;
        case Oarr:
            if (!dst)
                dst = temp(s, n);
            t = addr(s, dst, exprtype(dst));
            for (i = 0; i < n->expr.nargs; i++)
                assignat(s, t, size(n->expr.args[i])*i, n->expr.args[i]);
            r = dst;
            break;
        case Ostruct:
            if (!dst)
                dst = temp(s, n);
            t = addr(s, dst, exprtype(dst));
            for (i = 0; i < n->expr.nargs; i++)
                assignat(s, t, offset(n, n->expr.args[i]->expr.idx), n->expr.args[i]);
            r = dst;
            break;
        case Ocast:
            r = simpcast(s, args[0], exprtype(n));
            break;

        /* fused ops:
         * foo ?= blah
         *    =>
         *     foo = foo ? blah*/
        case Oaddeq: case Osubeq: case Omuleq: case Odiveq: case Omodeq:
        case Oboreq: case Obandeq: case Obxoreq: case Obsleq: case Obsreq:
            assert(fusedmap[exprop(n)] != Obad);
            u = rval(s, args[0], NULL);
            v = rval(s, args[1], NULL);
            v = mkexpr(n->line, fusedmap[exprop(n)], u, v, NULL);
            v->expr.type = u->expr.type;
            r = set(u, v);
            break;

        /* ++expr(x)
         *  => args[0] = args[0] + 1
         *     expr(x) */
        case Opreinc:
            v = assign(s, args[0], addk(args[0], 1));
            append(s, v);
            r = rval(s, args[0], NULL);
            break;
        case Opredec:
            v = assign(s, args[0], subk(args[0], 1));
            append(s, v);
            r = rval(s, args[0], NULL);
            break;

        /* expr(x++)
         *   => expr
         *      x = x + 1
         */
        case Opostinc:
            r = lval(s, args[0]);
            t = assign(s, r, addk(r, 1));
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Opostdec:
            r = lval(s, args[0]);
            t = assign(s, r, subk(r, 1));
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Olit:
            switch (args[0]->lit.littype) {
                case Lchr: case Lbool: case Llbl:
                    r = n;
                    break;
                case Lint: 
                    /* we can only have up to 4 byte immediates, but they
                     * can be moved into 64 bit regs */
                    if (args[0]->lit.intval < 0xffffffffULL)
                        r = n;
                    else
                        r = simpblob(s, n, &s->blobs, &s->nblobs);
                    break;
                case Lstr: case Lflt:
                    r = simpblob(s, n, &s->blobs, &s->nblobs);
                    break;
                case Lfunc:
                    r = simpblob(s, n, &file->file.stmts, &file->file.nstmts);
                    break;
            }
            break;
        case Ovar:
            r = n;
            break;
        case Oret:
            if (s->isbigret) {
                t = rval(s, args[0], NULL);
                t = addr(s, t, exprtype(args[0]));
                u = disp(n->line, size(args[0]));
                v = mkexpr(n->line, Oblit, s->ret, t, u, NULL);
                append(s, v);
            } else if (n->expr.nargs && n->expr.args[0]) {
                t = s->ret;
                t = set(t, rval(s, args[0], NULL));
                append(s, t);
            }
            /* drain the increment queue before we return */
            for (i = 0; i < s->nqueue; i++)
                append(s, s->incqueue[i]);
            lfree(&s->incqueue, &s->nqueue);
            jmp(s, s->endlbl);
            break;
        case Oasn:
            r = assign(s, args[0], args[1]);
            break;
        case Ocall:
            if (exprtype(n)->type != Tyvoid && stacktype(exprtype(n))) {
                r = temp(s, n);
                linsert(&n->expr.args, &n->expr.nargs, 1, addr(s, r, exprtype(n)));
                for (i = 0; i < n->expr.nargs; i++)
                    n->expr.args[i] = rval(s, n->expr.args[i], NULL);
                append(s, n);
            } else {
                r = visit(s, n);
            }
            break;
        case Oaddr:
            t = lval(s, args[0]);
            if (exprop(t) == Ovar) /* Ovar is the only one that doesn't return Oderef(Oaddr(...)) */
                r = addr(s, t, exprtype(t));
            else
                r = t->expr.args[0];
            break;
        case Oneg:
            if (istyfloat(exprtype(n))) {
                t =mkfloat(n->line, -1.0); 
                u = mkexpr(n->line, Olit, t, NULL);
                t->lit.type = n->expr.type;
                u->expr.type = n->expr.type;
                v = simpblob(s, u, &s->blobs, &s->nblobs);
                r = mkexpr(n->line, Ofmul, v, rval(s, args[0], NULL), NULL);
                r->expr.type = n->expr.type;
            } else {
                r = visit(s, n);
            }
            break;
        case Obreak:
            if (s->nloopexit == 0)
                fatal(n->line, "trying to break when not in loop");
            jmp(s, s->loopexit[s->nloopexit - 1]);
            break;
        case Ocontinue:
            if (s->nloopstep == 0)
                fatal(n->line, "trying to continue when not in loop");
            jmp(s, s->loopstep[s->nloopstep - 1]);
            break;
        case Oeq: case One: case Ogt: case Oge: case Olt: case Ole:
            if (istyfloat(tybase(exprtype(args[0]))))
                i = 1;
            else if (istysigned(tybase(exprtype(args[0]))))
                i = 0;
            else
                i = 2;
            n->expr.op = cmpmap[n->expr.op][i];
            r = visit(s, n);
            break;
        default:
            if (istyfloat(exprtype(n))) {
                switch (exprop(n)) {
                    case Oadd: n->expr.op = Ofadd; break;
                    case Osub: n->expr.op = Ofsub; break;
                    case Omul: n->expr.op = Ofmul; break;
                    case Odiv: n->expr.op = Ofdiv; break;
                    default: break;
                }
            }
            r = visit(s, n);
            break;
        case Obad:
            die("Bad operator");
            break;
    }
    return r;
}

static void declarelocal(Simp *s, Node *n)
{
    assert(n->type == Ndecl || (n->type == Nexpr && exprop(n) == Ovar));
    s->stksz += size(n);
    s->stksz = align(s->stksz, min(size(n), Ptrsz));
    if (debugopt['i']) {
        dump(n, stdout);
        printf("declared at %zd, size = %zd\n", s->stksz, size(n));
    }
    htput(s->stkoff, n, (void*)s->stksz);
}

static void declarearg(Simp *s, Node *n)
{
    assert(n->type == Ndecl || (n->type == Nexpr && exprop(n) == Ovar));
    s->argsz = align(s->argsz, min(size(n), Ptrsz));
    htput(s->stkoff, n, (void*)-(s->argsz + 2*Ptrsz));
    if (debugopt['i']) {
        dump(n, stdout);
        printf("declared at %zd\n", -(s->argsz + 2*Ptrsz));
    }
    s->argsz += size(n);
}

static int islbl(Node *n)
{
    Node *l;
    if (exprop(n) != Olit)
        return 0;
    l = n->expr.args[0];
    return l->type == Nlit && l->lit.littype == Llbl;
}

static Node *simp(Simp *s, Node *n)
{
    Node *r, *t, *u;
    size_t i;

    if (!n)
        return NULL;
    r = NULL;
    switch (n->type) {
        case Nblock:     simpblk(s, n);         break;
        case Nifstmt:    simpif(s, n, NULL);    break;
        case Nloopstmt:  simploop(s, n);        break;
        case Niterstmt:  simpiter(s, n);        break;
        case Nmatchstmt: simpmatch(s, n);       break;
        case Nexpr:
            if (islbl(n))
                append(s, n);
            else
                r = rval(s, n, NULL);
            if (r)
                append(s, r);
            /* drain the increment queue for this expr */
            for (i = 0; i < s->nqueue; i++)
                append(s, s->incqueue[i]);
            lfree(&s->incqueue, &s->nqueue);
            break;

        case Ndecl:
            declarelocal(s, n);
            if (!n->decl.init)
                break;
            t = mkexpr(n->line, Ovar, n->decl.name, NULL);
            u = mkexpr(n->line, Oasn, t, n->decl.init, NULL);
            u->expr.type = n->decl.type;
            t->expr.type = n->decl.type;
            t->expr.did = n->decl.did;
            simp(s, u);
            break;
        default:
            die("Bad node passsed to simp()");
            break;
    }
    return r;
}

/*
 * Turns a deeply nested function body into a flatter
 * and simpler representation, which maps easily and
 * directly to assembly instructions.
 */
static void flatten(Simp *s, Node *f)
{
    Node *dcl;
    Type *ty;
    size_t i;

    assert(f->type == Nfunc);
    s->nstmts = 0;
    s->stmts = NULL;
    s->endlbl = genlbl();
    s->ret = NULL;

    /* make a temp for the return type */
    ty = f->func.type->sub[0];
    if (stacktype(ty)) {
        s->isbigret = 1;
        s->ret = gentemp(s, f, mktyptr(f->line, ty), &dcl);
        declarearg(s, dcl);
    } else if (ty->type != Tyvoid) {
        s->isbigret = 0;
        s->ret = gentemp(s, f, ty, &dcl);
    }

    for (i = 0; i < f->func.nargs; i++) {
      declarearg(s, f->func.args[i]);
    }
    simp(s, f->func.body);

    append(s, s->endlbl);
}

static Func *simpfn(Simp *s, char *name, Node *n, Vis vis)
{
    size_t i;
    Func *fn;
    Cfg *cfg;

    if(debugopt['i'] || debugopt['F'] || debugopt['f'])
        printf("\n\nfunction %s\n", name);

    /* set up the simp context */
    /* unwrap to the function body */
    n = n->expr.args[0];
    n = n->lit.fnval;
    pushstab(n->func.scope);
    flatten(s, n);
    popstab();

    if (debugopt['f'] || debugopt['F'])
        for (i = 0; i < s->nstmts; i++)
            dump(s->stmts[i], stdout);
    for (i = 0; i < s->nstmts; i++) {
        if (s->stmts[i]->type != Nexpr)
            continue;
        if (debugopt['f']) {
            printf("FOLD FROM ----------\n");
            dump(s->stmts[i], stdout);
        }
        s->stmts[i] = fold(s->stmts[i], 0);
        if (debugopt['f']) {
            printf("TO ------------\n");
            dump(s->stmts[i], stdout);
            printf("DONE ----------------\n");
        }
    }

    cfg = mkcfg(s->stmts, s->nstmts);
    if (debugopt['t'] || debugopt['s'])
        dumpcfg(cfg, stdout);

    fn = zalloc(sizeof(Func));
    fn->name = strdup(name);
    if (vis != Visintern)
        fn->isexport = 1;
    fn->stksz = align(s->stksz, 8);
    fn->stkoff = s->stkoff;
    fn->ret = s->ret;
    fn->cfg = cfg;
    return fn;
}

static void fillglobls(Stab *st, Htab *globls)
{
    void **k;
    size_t i, nk;
    Stab *stab;
    Node *s;

    k = htkeys(st->dcl, &nk);
    for (i = 0; i < nk; i++) {
        s = htget(st->dcl, k[i]);
        htput(globls, s, asmname(s->decl.name));
    }
    free(k);

    k = htkeys(st->ns, &nk);
    for (i = 0; i < nk; i++) {
        stab = htget(st->ns, k[i]);
        fillglobls(stab, globls);
    }
    free(k);
}

static void extractsub(Simp *s, Node ***blobs, size_t *nblobs, Node *e)
{
    size_t i;

    switch (exprop(e)) {
        case Oslice:
            if (exprop(e->expr.args[0]) == Oarr)
                e->expr.args[0] = simpblob(s, e->expr.args[0], blobs, nblobs);
            break;
        case Oarr:
        case Ostruct:
            for (i = 0; i < e->expr.nargs; i++)
                extractsub(s, blobs, nblobs, e->expr.args[i]);
            break;
        default:
            break;
    }
}

static void simpconstinit(Simp *s, Node *dcl)
{
    Node *e;

    dcl->decl.init = fold(dcl->decl.init, 1);;
    e = dcl->decl.init;
    if (e && exprop(e) == Olit) {
        if (e->expr.args[0]->lit.littype == Lfunc)
            simpblob(s, e, &file->file.stmts, &file->file.nstmts);
        else
            lappend(&s->blobs, &s->nblobs, dcl);
    } else if (dcl->decl.isconst) {
        switch (exprop(e)) {
            case Oarr:
            case Ostruct:
            case Oslice:
                extractsub(s, &s->blobs, &s->nblobs, e);
                lappend(&s->blobs, &s->nblobs, dcl);
                break;
            default:
                break;
        }
    } else if (!dcl->decl.isconst && !e) {
        lappend(&s->blobs, &s->nblobs, dcl);
    } else {
        die("Non-constant initializer for %s\n", declname(dcl));
    }
}

static void simpglobl(Node *dcl, Htab *globls, Func ***fn, size_t *nfn, Node ***blob, size_t *nblob)
{
    Simp s = {0,};
    char *name;
    Func *f;

    name = asmname(dcl->decl.name);
    s.stkoff = mkht(varhash, vareq);
    s.globls = globls;
    s.blobs = *blob;
    s.nblobs = *nblob;

    if (dcl->decl.isextern || dcl->decl.isgeneric)
        return;
    if (isconstfn(dcl)) {
        f = simpfn(&s, name, dcl->decl.init, dcl->decl.vis);
        lappend(fn, nfn, f);
    } else {
        simpconstinit(&s, dcl);
    }
    *blob = s.blobs;
    *nblob = s.nblobs;
    free(name);
}

void gen(Node *file, char *out)
{
    Htab *globls, *strtab;
    Node *n, **blob;
    Func **fn;
    size_t nfn, nblob;
    size_t i;
    FILE *fd;

    /* declare useful constants */
    tyintptr = mktype(-1, Tyuint64);
    tyword = mktype(-1, Tyuint);
    tyvoid = mktype(-1, Tyvoid);

    fn = NULL;
    nfn = 0;
    blob = NULL;
    nblob = 0;
    globls = mkht(varhash, vareq);

    /* We need to define all global variables before use */
    fillglobls(file->file.globls, globls);

    pushstab(file->file.globls);
    for (i = 0; i < file->file.nstmts; i++) {
        n = file->file.stmts[i];
        switch (n->type) {
            case Nuse: /* nothing to do */ 
            case Nimpl:
                break;
            case Ndecl:
                simpglobl(n, globls, &fn, &nfn, &blob, &nblob);
                break;
            default:
                die("Bad node %s in toplevel", nodestr(n->type));
                break;
        }
    }
    popstab();

    fd = fopen(out, "w");
    if (!fd)
        die("Couldn't open fd %s", out);

    strtab = mkht(strhash, streq);
    fprintf(fd, ".data\n");
    for (i = 0; i < nblob; i++)
        genblob(fd, blob[i], globls, strtab);
    fprintf(fd, ".text\n");
    for (i = 0; i < nfn; i++)
        genasm(fd, fn[i], globls, strtab);
    genstrings(fd, strtab);
    fclose(fd);
}
