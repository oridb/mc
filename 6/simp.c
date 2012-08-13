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

    /* location handling */
    Node **blobs;
    size_t nblobs;
    size_t stksz;
    size_t argsz;
    Htab *globls;
    Htab *locs;
};

static Node *simp(Simp *s, Node *n);
static Node *rval(Simp *s, Node *n, Node *dst);
static Node *lval(Simp *s, Node *n);
static void declarelocal(Simp *s, Node *n);
static void simpcond(Simp *s, Node *n, Node *ltrue, Node *lfalse);

/* useful constants */
static Type *tyintptr;
static Type *tyword;
static Type *tyvoid;

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

static Node *addr(Node *a, Type *bt)
{
    Node *n;

    n = mkexpr(a->line, Oaddr, a, NULL);
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
    n = mkexpr(a->line, Oload, a, NULL);
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

static char *asmname(Node *n)
{
    char *s;
    int len;

    len = strlen(Fprefix);
    if (n->name.ns)
        len += strlen(n->name.ns) + 1; /* +1 for separator */
    len += strlen(n->name.name);

    s = xalloc(len + 1);
    s[0] = '\0';
    sprintf(s, "%s", Fprefix);
    if (n->name.ns)
        sprintf(s, "%s%s$", s, n->name.ns);
    sprintf(s, "%s%s", s, n->name.name);
    return s;
}

int stacktype(Type *t)
{
    /* the types are arranged in types.def such that this is true */
    t = tybase(t);
    return t->type >= Tyslice;
}

int stacknode(Node *n)
{
    if (n->type == Nexpr)
        return stacktype(n->expr.type);
    else
        return stacktype(n->decl.type);
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
            assert(exprop(t->asize) == Olit);
            return t->asize->expr.args[0]->lit.intval * tysize(t->sub[0]);
        case Tytuple:
            for (i = 0; i < t->nsub; i++)
                sz += tysize(t->sub[i]);
            return sz;
            break;
        case Tystruct:
            for (i = 0; i < t->nmemb; i++)
                sz += align(size(t->sdecls[i]), Ptrsz);
            return sz;
            break;
        case Tyunion:
            sz = Ptrsz;
            for (i = 0; i < t->nmemb; i++)
                if (t->udecls[i]->etype)
                    sz = max(sz, tysize(t->udecls[i]->etype) + Ptrsz);
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
 *       :cond
 *           ...cond...
 *       cjmp (cond) :body :end
 *       :end
 */
static void simploop(Simp *s, Node *n)
{
    Node *lbody;
    Node *lend;
    Node *lcond;

    lbody = genlbl();
    lcond = genlbl();
    lend = genlbl();

    simp(s, n->loopstmt.init);  /* init */
    jmp(s, lcond);              /* goto test */
    simp(s, lbody);             /* body lbl */
    simp(s, n->loopstmt.body);  /* body */
    simp(s, n->loopstmt.step);  /* step */
    simp(s, lcond);             /* test lbl */
    simpcond(s, n->loopstmt.cond, lbody, lend);    /* repeat? */
    simp(s, lend);              /* exit */
}

static Ucon *finducon(Node *n)
{
    size_t i;
    Type *t;
    Ucon *uc;

    t = tybase(n->expr.type);
    if (exprop(n) != Ocons)
        return NULL;
    for (i = 0; i  < t->nmemb; i++) {
        uc = t->udecls[i];
        if (!strcmp(namestr(uc->name), namestr(n->expr.args[0])))
            return uc;
    }
    die("No ucon?!?");
    return NULL;
}

static Node *uconid(Node *n, size_t off)
{
    Ucon *uc;

    if (exprop(n) != Ocons)
        return load(addk(addr(n, mkty(n->line, Tyuint)), off));

    uc = finducon(n);
    return word(uc->line, uc->id);
}

static Node *uval(Node *n, size_t off, Type *t)
{
    if (exprop(n) == Ocons)
        return n->expr.args[1];
    else if (exprop(n) == Olit)
        return n;
    else
        return load(addk(addr(n, t), off));
}

static void ucompare(Simp *s, Node *a, Node *b, Type *t, size_t off, Node *iftrue, Node *iffalse)
{
    Node *v, *x, *y;
    Node *next;
    Ucon *uc;

    assert(a->type == Nexpr);
    t = tybase(t);
    switch (t->type) {
        case Tyvoid: case Tybad: case Tyvalist: case Tyvar:
        case Typaram: case Tyunres: case Tyname: case Ntypes:
        case Tyint64: case Tyuint64: case Tylong:  case Tyulong:
        case Tyfloat32: case Tyfloat64:
        case Tyslice: case Tyarray: case Tytuple: case Tystruct:
            die("Unsupported type for compare");
            break;
        case Tybool: case Tychar: case Tybyte:
        case Tyint8: case Tyint16: case Tyint32: case Tyint:
        case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint:
        case Typtr: case Tyfunc:
            x = uval(a, off, t);
            y = uval(b, off, t);
            v = mkexpr(a->line, Oeq, x, y, NULL);
            cjmp(s, v, iftrue, iffalse);
            break;
        case Tyunion:
            x = uconid(a, off);
            y = uconid(b, off);
            uc = finducon(a);
            if (!uc)
                uc = finducon(b);

            next = genlbl();
            v = mkexpr(a->line, Oeq, x, y, NULL);
            v->expr.type = tyintptr;
            cjmp(s, v, next, iffalse);
            append(s, next);
            if (uc->etype) {
                off += Wordsz;
                ucompare(s, a, b, uc->etype, off, iftrue, iffalse);
            }
            break;
    }
}

FILE *f;
static void simpmatch(Simp *s, Node *n)
{
    Node *end, *cur, *next; /* labels */
    Node *val, *tmp;
    Node *m;
    size_t i;
    f = stdout;

    end = genlbl();
    val = temp(s, n->matchstmt.val);
    tmp = rval(s, n->matchstmt.val, val);
    if (val != tmp)
        append(s, set(val, tmp));
    for (i = 0; i < n->matchstmt.nmatches; i++) {
        m = n->matchstmt.matches[i];

        /* check pattern */
        cur = genlbl();
        next = genlbl();
        ucompare(s, val, m->match.pat, val->expr.type, 0, cur, next);

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

    for (i = 0; i < n->block.nstmts; i++) {
        simp(s, n->block.stmts[i]);
    }
}

static Node *simplit(Simp *s, Node *lit, Node ***l, size_t *nl)
{
    Node *n, *d, *r;
    char lbl[128];

    n = mkname(lit->line, genlblstr(lbl, 128));
    d = mkdecl(lit->line, n, lit->expr.type);
    r = mkexpr(lit->line, Ovar, n, NULL);

    d->decl.init = lit;
    d->decl.type = lit->expr.type;
    d->decl.isconst = 1;
    r->expr.did = d->decl.did;
    r->expr.type = lit->expr.type;
    if (tybase(r->expr.type)->type == Tyfunc)
        r = addr(r, tybase(r->expr.type));

    htput(s->globls, d, strdup(lbl));
    lappend(l, nl, d);
    return r;
}

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
        t = args[0];
    } else {
        t = addr(args[0], exprtype(n));
    }
    u = disp(n->line, offset(args[0], args[1]));
    r = add(t, u);
    r->expr.type = mktyptr(n->line, n->expr.type);
    return r;
}

static Node *idxaddr(Simp *s, Node *n)
{
    Node *a, *t, *u, *v; /* temps */
    Node *r; /* result */
    Node **args;
    size_t sz;

    assert(exprop(n) == Oidx);
    args = n->expr.args;
    a = rval(s, args[0], NULL);
    if (exprtype(args[0])->type == Tyarray)
        t = addr(a, exprtype(n));
    else if (args[0]->expr.type->type == Tyslice)
        t = load(addr(a, mktyptr(n->line, exprtype(n))));
    else
        die("Can't index type %s\n", tystr(n->expr.type));
    assert(t->expr.type->type == Typtr);
    u = rval(s, args[1], NULL);
    u = ptrsized(s, u);
    sz = size(n);
    v = mul(u, disp(n->line, sz));
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
        case Typtr:     u = n; break;
        case Tyarray:   u = addr(t, base(exprtype(n))); break;
        case Tyslice:   u = load(addr(t, mktyptr(n->line, base(exprtype(n))))); break;
        default: die("Unslicable type %s", tystr(n->expr.type));
    }
    /* safe: all types we allow here have a sub[0] that we want to grab */
    if (off) {
      off = ptrsized(s, off);
      sz = tysize(n->expr.type->sub[0]);
      v = mul(off, disp(n->line, sz));
      return add(u, v);
    } else {
      return u;
    }
}

static Node *slicelen(Simp *s, Node *sl)
{
    /* *(&sl + sizeof(size_t)) */
    return load(addk(addr(sl, tyintptr), Ptrsz));
}

Node *lval(Simp *s, Node *n)
{
    Node *r;

    switch (exprop(n)) {
        case Ovar:      r = n;  break;
        case Oidx:      r = deref(idxaddr(s, n)); break;
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

static Node *simpcast(Simp *s, Node *val, Type *to)
{
    Node *r;
    Type *t;
    int issigned;
    size_t fromsz, tosz;

    issigned = 0;
    r = NULL;
    switch (tybase(to)->type) {
        case Tyint8: case Tyint16: case Tyint32: case Tyint64:
        case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
        case Tyint: case Tyuint: case Tylong: case Tyulong:
        case Tychar: case Tybyte:
        case Typtr:
            t = tybase(exprtype(val));
            switch (t->type) {
                case Tyslice:
                    if (t->type == Typtr)
                        fatal(val->line, "Bad cast from %s to %s",
                              tystr(exprtype(val)), tystr(to));
                    r = slicebase(s, val, NULL);
                    break;
                case Tyint8: case Tyint16: case Tyint32: case Tyint64:
                case Tyint: case Tylong:
                    issigned = 1;
                case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
                case Tyuint: case Tyulong: case Tychar: case Tybyte:
                case Typtr:
                    fromsz = size(val);
                    tosz = tysize(to);
                    r = rval(s, val, NULL);
                    if (fromsz > tosz) {
                        r = mkexpr(val->line, Otrunc, r, NULL);
                    } else if (tosz > fromsz) {
                        if (issigned)
                            r = mkexpr(val->line, Oswiden, r, NULL);
                        else
                            r = mkexpr(val->line, Ozwiden, r, NULL);
                    }
                    r->expr.type = to;
                    break;
                default:
                    fatal(val->line, "Bad cast from %s to %s",
                          tystr(exprtype(val)), tystr(to));
            }
            break;
        default:
            fatal(val->line, "Bad cast from %s to %s",
                  tystr(exprtype(val)), tystr(to));
    }
    return r;
}

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
        stbase = set(deref(addr(t, tyintptr)), base);
        sz = addk(addr(t, tyintptr), Ptrsz);
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

Node *destructure(Simp *s, Node *lhs, Node *rhs)
{
    Node *plv, *prv, *lv, *sz, *stor, **args;
    size_t off, i;

    args = lhs->expr.args;
    rhs = rval(s, rhs, NULL);
    off = 0;
    for (i = 0; i < lhs->expr.nargs; i++) {
        lv = lval(s, args[i]);
        prv = add(addr(rhs, exprtype(args[i])), disp(rhs->line, off));
        if (stacknode(args[i])) {
            sz = disp(lhs->line, size(lv));
            plv = addr(lv, exprtype(lv));
            stor = mkexpr(lhs->line, Oblit, plv, prv, sz, NULL);
        } else {
            stor = set(lv, load(prv));
        }
        append(s, stor);
        off += size(lv);
    }

    return NULL;
}

Node *assign(Simp *s, Node *lhs, Node *rhs)
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
            t = addr(t, exprtype(lhs));
            u = addr(u, exprtype(lhs));
            v = disp(lhs->line, size(lhs));
            r = mkexpr(lhs->line, Oblit, t, u, v, NULL);
        } else {
            r = set(t, u);
        }
    }
    return r;
}

static Node *simptup(Simp *s, Node *n, Node *dst)
{
    Node *pdst, *pval, *val, *sz, *st, **args;
    Node *r;
    size_t i, off;

    args = n->expr.args;
    if (!dst)
        dst = temp(s, n);
    r = addr(dst, exprtype(dst));

    off = 0;
    for (i = 0; i < n->expr.nargs; i++) {
        val = rval(s, args[i], NULL);
        pdst = add(r, disp(n->line, off));
        if (stacknode(args[i])) {
            sz = disp(n->line, size(val));
            pval = addr(val, exprtype(val));
            st = mkexpr(n->line, Oblit, pdst, pval, sz, NULL);
        } else {
            st = set(deref(pdst), val);
        }
        append(s, st);
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
    u = addr(tmp, mkty(n->line, Tyuint));
    tag = mkintlit(n->line, uc->id);
    tag->expr.type = mkty(n->line, Tyuint);
    append(s, set(deref(u), tag));


    /* fill the value, if needed */
    if (!uc->etype)
        return tmp;
    elt = rval(s, n->expr.args[1], NULL);
    u = addk(u, Wordsz);
    if (stacktype(uc->etype)) {
        elt = addr(elt, uc->etype);
        sz = disp(n->line, tysize(uc->utype));
        r = mkexpr(n->line, Oblit, u, elt, sz, NULL);
    } else {
        r = set(deref(u), elt);
    }
    append(s, r);
    return tmp;
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
    u->expr.type = mkty(n->line, Tybool);
    t = set(r, u);
    append(s, t);
    jmp(s, ldone);

    /* if false */
    append(s, lfalse);
    u = mkexpr(n->line, Olit, mkbool(n->line, 0), NULL);
    u->expr.type = mkty(n->line, Tybool);
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


    r = NULL;
    args = n->expr.args;
    switch (exprop(n)) {
        case Obad:
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
            t = idxaddr(s, n);
            r = load(t);
            break;
        case Omemb:
            if (exprtype(args[0])->type == Tyslice) {
                assert(!strcmp(namestr(args[1]), "len"));
                t = slicelen(s, args[0]);
                r = simpcast(s, t, exprtype(n));
            } else if (exprtype(args[0])->type == Tyarray) {
                assert(!strcmp(namestr(args[1]), "len"));
                t = exprtype(args[0])->asize;
                r = simpcast(s, t, exprtype(n));
            } else {
                t = membaddr(s, n);
                r = load(t);
            }
            break;
        case Ocons:
            r = simpucon(s, n, dst);
            break;
        case Otup:
            r = simptup(s, n, dst);
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
         *     =>
         *      expr
         *      x = x + 1
         */
        case Opostinc:
            t = assign(s, args[0], addk(args[0], 1));
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Opostdec:
            t = assign(s, args[0], subk(args[0], 1));
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Olit:
            switch (args[0]->lit.littype) {
                case Lchr: case Lbool: case Lint:
                    r = n;
                    break;
                case Lstr: case Lseq: case Lflt:
                    r = simplit(s, n, &s->blobs, &s->nblobs);
                    break;
                case Lfunc:
                    r = simplit(s, n, &file->file.stmts, &file->file.nstmts);
                    break;
            }
            break;
        case Ovar:
            r = n;
            break;
        case Oret:
            if (s->isbigret) {
                t = rval(s, args[0], NULL);
                t = addr(t, exprtype(args[0]));
                u = disp(n->line, size(args[0]));
                v = mkexpr(n->line, Oblit, s->ret, t, u, NULL);
                append(s, v);
            } else if (n->expr.nargs && n->expr.args[0]) {
                t = s->ret;
                t = set(t, rval(s, args[0], NULL));
                append(s, t);
            }
            jmp(s, s->endlbl);
            break;
        case Oasn:
            r = assign(s, args[0], args[1]);
            break;
        case Ocall:
            if (exprtype(n)->type != Tyvoid && size(n) > Ptrsz) {
                r = temp(s, n);
                linsert(&n->expr.args, &n->expr.nargs, 1, addr(r, exprtype(n)));
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
                r = addr(t, exprtype(t));
            else
                r = t->expr.args[0];
            break;
        default:
            r = visit(s, n);
    }
    return r;
}

static void declarelocal(Simp *s, Node *n)
{
    assert(n->type == Ndecl);
    s->stksz += size(n);
    s->stksz = align(s->stksz, min(size(n), Ptrsz));
    if (debug)
        printf("declare %s:%s(%zd) at %zd\n", declname(n), tystr(decltype(n)), n->decl.did, s->stksz);
    htput(s->locs, n, (void*)s->stksz);
}

static void declarearg(Simp *s, Node *n)
{
    assert(n->type == Ndecl);
    s->argsz = align(s->argsz, min(size(n), Ptrsz));
    if (debug)
        printf("declare %s(%zd) at %zd\n", declname(n), n->decl.did, -(s->argsz + 2*Ptrsz));
    htput(s->locs, n, (void*)-(s->argsz + 2*Ptrsz));
    s->argsz += size(n);
}

static Node *simp(Simp *s, Node *n)
{
    Node *r, *t, *u;
    size_t i;

    if (!n)
        return NULL;
    r = NULL;
    switch (n->type) {
        case Nlit:       r = n;                 break;
        case Nlbl:       append(s, n);          break;
        case Nblock:     simpblk(s, n);         break;
        case Nifstmt:    simpif(s, n, NULL);    break;
        case Nloopstmt:  simploop(s, n);        break;
        case Nmatchstmt: simpmatch(s, n);       break;
        case Nexpr:
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
            if (n->decl.init) {
                t = mkexpr(n->line, Ovar, n->decl.name, NULL);
                u = mkexpr(n->line, Oasn, t, n->decl.init, NULL);
                u->expr.type = n->decl.type;
                t->expr.type = n->decl.type;
                t->expr.did = n->decl.did;
                simp(s, u);
            }
            break;
        default:
            die("Bad node passsed to simp()");
            break;
    }
    return r;
}

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

    assert(f->type == Nfunc);

    ty = f->func.type->sub[0];
    if (stacktype(ty)) {
        s->isbigret = 1;
        s->ret = gentemp(s, f, mktyptr(f->line, ty), &dcl);
        declarearg(s, dcl);
    } else if (ty->type != Tyvoid) {
        s->isbigret = 0;
        s->ret = gentemp(s, f, ty, &dcl);
        declarelocal(s, dcl);
    }

    for (i = 0; i < f->func.nargs; i++) {
      declarearg(s, f->func.args[i]);
    }
    simp(s, f->func.body);

    append(s, s->endlbl);
}

static Func *simpfn(Simp *s, char *name, Node *n, int export)
{
    size_t i;
    Func *fn;
    Cfg *cfg;

    if(debug)
        printf("\n\nfunction %s\n", name);

    if (!n->decl.init)
        return NULL;
    /* set up the simp context */
    /* unwrap to the function body */
    n = n->expr.args[0];
    n = n->lit.fnval;
    flatten(s, n);

    if (debug)
        for (i = 0; i < s->nstmts; i++)
            dump(s->stmts[i], stdout);
    for (i = 0; i < s->nstmts; i++) {
        if (s->stmts[i]->type != Nexpr)
            continue;
        if (debugopt['f']) {
            printf("FOLD FROM ----------\n");
            dump(s->stmts[i], stdout);
        }
        s->stmts[i] = fold(s->stmts[i]);
        if (debugopt['f']) {
            printf("FOLD TO ------------\n");
            dump(s->stmts[i], stdout);
            printf("END ----------------\n");
        }
    }
    cfg = mkcfg(s->stmts, s->nstmts);
    if (debug)
        dumpcfg(cfg, stdout);

    fn = zalloc(sizeof(Func));
    fn->name = strdup(name);
    fn->isexport = export;
    fn->stksz = align(s->stksz, 8);
    fn->locs = s->locs;
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

static void simpdcl(Node *dcl, Htab *globls, Func ***fn, size_t *nfn, Node ***blob, size_t *nblob)
{
    Simp s = {0,};
    char *name;
    Func *f;

    name = asmname(dcl->decl.name);
    s.locs = mkht(dclhash, dcleq);
    s.globls = globls;
    s.blobs = *blob;
    s.nblobs = *nblob;

    if (isconstfn(dcl)) {
        if (!dcl->decl.isextern && !dcl->decl.isgeneric) {
            f = simpfn(&s, name, dcl->decl.init, dcl->decl.isexport);
            lappend(fn, nfn, f);
        }
    } else {
        dcl->decl.init = fold(dcl->decl.init);
        if (dcl->decl.init && exprop(dcl->decl.init) == Olit)
            lappend(&s.blobs, &s.nblobs, dcl);
        /* uninitialized global vars get zero-initialized decls */
        else if (!dcl->decl.isconst && !dcl->decl.init)
            lappend(&s.blobs, &s.nblobs, dcl);
        else
            die("We don't simp globls with nonlit inits yet...");
    }
    *blob = s.blobs;
    *nblob = s.nblobs;
    free(name);
}

void gen(Node *file, char *out)
{
    Htab *globls;
    Node *n, **blob;
    Func **fn;
    size_t nfn, nblob;
    size_t i;
    FILE *fd;

    /* declare useful constants */
    tyintptr = mkty(-1, Tyuint64);
    tyword = mkty(-1, Tyuint);
    tyvoid = mkty(-1, Tyvoid);

    fn = NULL;
    nfn = 0;
    blob = NULL;
    nblob = 0;
    globls = mkht(dclhash, dcleq);

    /* We need to define all global variables before use */
    fillglobls(file->file.globls, globls);

    for (i = 0; i < file->file.nstmts; i++) {
        n = file->file.stmts[i];
        switch (n->type) {
            case Nuse: /* nothing to do */ 
                break;
            case Ndecl:
                simpdcl(n, globls, &fn, &nfn, &blob, &nblob);
                break;
            default:
                die("Bad node %s in toplevel", nodestr(n->type));
                break;
        }
    }

    fd = fopen(out, "w");
    if (!fd)
        die("Couldn't open fd %s", out);

    fprintf(fd, ".data\n");
    for (i = 0; i < nblob; i++)
        genblob(fd, blob[i], globls);
    fprintf(fd, ".text\n");
    for (i = 0; i < nfn; i++)
        genasm(fd, fn[i], globls);
    fclose(fd);
}
