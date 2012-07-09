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

/* useful constants */
static Node *one;
static Node *zero;
static Node *ptrsz;
static Node *wordsz;
static Type *tyword;
static Type *tyvoid;

static int max(int a, int b)
{
    if (a > b)
        return a;
    else
        return b;
}

static Type *base(Type *t)
{
    assert(t->nsub == 1);
    return t->sub[0];
}

static Node *add(Node *a, Node *b)
{
    Node *n;

    n = mkexpr(a->line, Oadd, a, b, NULL);
    n->expr.type = a->expr.type;
    return n;
}

static Node *sub(Node *a, Node *b)
{
    Node *n;

    n = mkexpr(a->line, Osub, a, b, NULL);
    n->expr.type = a->expr.type;
    return n;
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

    n = mkexpr(a->line, Oload, a, NULL);
    assert(a->expr.type->type == Typtr);
    n->expr.type = base(a->expr.type);
    return n;
}

static Node *store(Node *a, Node *b)
{
    Node *n;

    assert(a != NULL && b != NULL);
    n = mkexpr(a->line, Ostor, a, b, NULL);
    return n;
}

static Node *word(int line, uint v)
{
    Node *n;
    n = mkintlit(line, v);
    n->expr.type = tyword;
    return n;
}


static size_t did(Node *n)
{
    if (n->type == Ndecl) {
        return n->decl.did;
    } else if (n->type == Nexpr) {
        assert(exprop(n) == Ovar);
        return n->expr.did;
    }
    dump(n, stderr);
    die("Can't get did");
    return 0;
}

static ulong dclhash(void *dcl)
{
    /* large-prime hash. meh. */
    return did(dcl) * 366787;
}

static int dcleq(void *a, void *b)
{
    return did(a) == did(b);
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

size_t tysize(Type *t)
{
    size_t sz;
    size_t i;

    sz = 0;
    if (!t)
        return 0;
    switch (t->type) {
        case Tyvoid:
            die("void has no size");
            return 1;
        case Tybool: case Tychar: case Tyint8:
        case Tybyte: case Tyuint8:
            return 1;
        case Tyint16: case Tyuint16:
            return 2;
        case Tyint: case Tyint32:
        case Tyuint: case Tyuint32:
        case Typtr: case Tyfunc:
            return 4;

        case Tyint64: case Tylong:
        case Tyuint64: case Tyulong:
            return 8;

            /*end integer types*/
        case Tyfloat32:
            return 4;
        case Tyfloat64:
            return 8;
        case Tyvalist:
            return 4; /* ptr to first element of valist */

        case Tyslice:
            return 8; /* len; ptr */
        case Tyalias:
            return tysize(t->sub[0]);
        case Tyarray:
            assert(exprop(t->asize) == Olit);
            return t->asize->expr.args[0]->lit.intval * tysize(t->sub[0]);
        case Tytuple:
        case Tystruct:
            for (i = 0; i < t->nmemb; i++)
                sz += size(t->sdecls[i]);
            return sz;
            break;
        case Tyunion:
            sz = Wordsz;
            for (i = 0; i < t->nmemb; i++)
                sz = max(sz, tysize(t->udecls[i]->etype) + Wordsz);
            return sz;
            break;
        case Tybad: case Tyvar: case Typaram: case Tyname: case Ntypes:
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
static void simpif(Simp *s, Node *n)
{
    Node *l1, *l2;
    Node *c;

    l1 = genlbl();
    l2 = genlbl();
    c = rval(s, n->ifstmt.cond, NULL);
    cjmp(s, c, l1, l2);
    simp(s, l1);
    simp(s, n->ifstmt.iftrue);
    simp(s, l2);
    simp(s, n->ifstmt.iffalse);
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
    Node *t;

    lbody = genlbl();
    lcond = genlbl();
    lend = genlbl();

    simp(s, n->loopstmt.init);  /* init */
    jmp(s, lcond);              /* goto test */
    simp(s, lbody);             /* body lbl */
    simp(s, n->loopstmt.body);  /* body */
    simp(s, n->loopstmt.step);  /* step */
    simp(s, lcond);             /* test lbl */
    t = rval(s, n->loopstmt.cond, NULL);  /* test */
    cjmp(s, t, lbody, lend);    /* repeat? */
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
    Type *t;

    t = tybase(n->expr.type);
    if (exprop(n) != Ocons)
        return load(add(addr(n, tyword), word(n->line, off)));

    uc = finducon(n);
    return word(uc->line, uc->id);
}

static Node *uval(Node *n, size_t off)
{
    if (exprop(n) == Ocons)
        return n->expr.args[1];
    else if (exprop(n) == Olit)
        return n;
    else
        /* FIXME: WRONG WRONG WRONG. Union vals 
         * aren't only words. */
        return load(add(addr(n, tyword), word(n->line, off)));
}

static Node *ucompare(Simp *s, Node *a, Node *b, Type *t, size_t off)
{
    Node *r, *v, *x, *y;
    Ucon *uc;

    assert(a->type == Nexpr);
    t = tybase(t);
    r = NULL;
    switch (t->type) {
        case Tyvoid: case Tybad: case Tyvalist: case Tyvar:
        case Typaram: case Tyname: case Tyalias: case Ntypes:
        case Tyint64: case Tyuint64: case Tylong:  case Tyulong:
        case Tyfloat32: case Tyfloat64:
        case Tyslice: case Tyarray: case Tytuple: case Tystruct:
            die("Unsupported type for compare");
            break;
        case Tybool: case Tychar: case Tybyte:
        case Tyint8: case Tyint16: case Tyint32: case Tyint:
        case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint:
        case Typtr: case Tyfunc:
            x = uval(a, off);
            y = uval(b, off);
            r = mkexpr(a->line, Oeq, x, y, NULL);
            r->expr.type = tyword;
            break;
        case Tyunion:
            x = uconid(a, off);
            y = uconid(b, off);
            uc = finducon(a);
            if (!uc)
                uc = finducon(b);

            r = mkexpr(a->line, Oeq, x, y, NULL);
            r->expr.type = tyword;
            if (uc->etype) {
                off += Wordsz;
                v = ucompare(s, a, b, uc->etype, off);
                r = mkexpr(a->line, Oland, r, v, NULL);
                r->expr.type = tyword;
                r = rval(s, r, NULL); /* Oland needs to be reduced */
            }
            break;
    }
    return r;
            
}


FILE *f;
static void simpmatch(Simp *s, Node *n)
{
    Node *end, *cur, *next; /* labels */
    Node *val, *cond; /* intermediates */
    Node *m;
    size_t i;
    f = stdout;

    end = genlbl();
    val = rval(s, n->matchstmt.val, NULL); /* FIXME: don't recompute, even if pure */
    for (i = 0; i < n->matchstmt.nmatches; i++) {
        m = n->matchstmt.matches[i];

        /* check pattern */
        cond = ucompare(s, val, m->match.pat, val->expr.type, 0);
        cur = genlbl();
        next = genlbl();
        cjmp(s, cond, cur, next);

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

static Node *bloblit(Simp *s, Node *lit)
{
    Node *n, *t, *r;
    char lbl[128];

    n = mkname(lit->line, genlblstr(lbl, 128));
    t = mkdecl(lit->line, n, lit->expr.type);
    r = mkexpr(lit->line, Ovar, n, NULL);
    r->expr.type = lit->expr.type;
    r->expr.did = t->decl.did;
    t->decl.init = lit;
    htput(s->globls, t, strdup(lbl));
    lappend(&s->blobs, &s->nblobs, t);
    return r;
}

static size_t offsetof(Node *aggr, Node *memb)
{
    Type *ty;
    Node **nl;
    size_t i;
    size_t off;

    if (aggr->expr.type->type == Typtr)
        aggr = aggr->expr.args[0];
    ty = aggr->expr.type;
    ty = tybase(ty);

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

static Node *membaddr(Simp *s, Node *n)
{
    Node *t, *u, *r;
    Node **args;

    args = n->expr.args;
    if (n->expr.type->type == Typtr) {
        t = args[0];
        t->expr.type = mktyptr(n->line, exprtype(n));
    } else {
        t = addr(args[0], exprtype(n));
    }
    u = word(n->line, offsetof(args[0], args[1]));
    r = add(t, u);
    r->expr.type = mktyptr(n->line, n->expr.type);
    return r;
}

static Node *idxaddr(Simp *s, Node *n)
{
    Node *t, *u, *v; /* temps */
    Node *r; /* result */
    Node **args;
    size_t sz;

    assert(exprop(n) == Oidx);
    args = n->expr.args;
    if (exprtype(args[0])->type == Tyarray)
        t = addr(args[0], exprtype(n));
    else if (args[0]->expr.type->type == Tyslice)
        t = load(addr(args[0], mktyptr(n->line, exprtype(n))));
    else
        die("Can't index type %s\n", tystr(n->expr.type));
    assert(t->expr.type->type == Typtr);
    u = rval(s, args[1], NULL);
    sz = size(n);
    v = mul(u, word(n->line, sz));
    r = add(t, v);
    return r;
}

static Node *slicebase(Simp *s, Node *n, Node *off)
{
    Node *t, *u, *v;
    int sz;

    t = rval(s, n, NULL);
    u = NULL;
    switch (exprtype(n)->type) {
        case Typtr:     u = n; break;
        case Tyarray:   u = addr(t, base(exprtype(n))); break;
        case Tyslice:   u = load(addr(t, mktyptr(n->line, base(exprtype(n))))); break;
        default: die("Unslicable type %s", tystr(n->expr.type));
    }
    /* safe: all types we allow here have a sub[0] that we want to grab */
    sz = tysize(n->expr.type->sub[0]);
    v = mul(off, word(n->line, sz));
    return add(u, v);
}

static Node *slicelen(Simp *s, Node *sl)
{
    /* *(&sl + 4) */
    return load(add(addr(sl, tyword), ptrsz));
}

Node *lval(Simp *s, Node *n)
{
    Node *r;

    switch (exprop(n)) {
        case Ovar:      r = n;  break;
        case Oidx:      r = idxaddr(s, n);      break;
        case Omemb:     r = membaddr(s, n);     break;
        default:
            die("%s cannot be an lval", opstr(exprop(n)));
            break;
    }
    return r;
}

static Node *simplazy(Simp *s, Node *n, Node *r)
{
    Node *a, *b;
    Node *next;
    Node *end;

    next = genlbl();
    end = genlbl();
    a = rval(s, n->expr.args[0], NULL);
    append(s, store(r, a));
    if (exprop(n) == Oland)
        cjmp(s, a, next, end);
    else if (exprop(n) == Olor)
        cjmp(s, a, end, next);
    append(s, next);
    b = rval(s, n->expr.args[1], NULL);
    append(s, store(r, b));
    append(s, end);
    return r;
}

static Node *lowerslice(Simp *s, Node *n, Node *dst)
{
    Node *t;
    Node *base, *sz, *len;
    Node *stbase, *stlen;

    if (dst)
        t = dst;
    else
        t = temp(s, n);
    /* *(&slice) = (void*)base + off*sz */
    base = slicebase(s, n->expr.args[0], n->expr.args[1]);
    len = sub(n->expr.args[2], n->expr.args[1]);
    stbase = store(addr(t, tyword), base);
    /* *(&slice + ptrsz) = len */
    sz = add(addr(t, tyword), ptrsz);
    stlen = store(sz, len);
    append(s, stbase);
    append(s, stlen);
    return t;
}

static Node *lowercast(Simp *s, Node *n)
{
    Node **args;
    Node *r;

    r = NULL;
    args = n->expr.args;
    switch (exprtype(n)->type) {
        case Typtr:
            switch (exprtype(args[0])->type) {
                case Tyslice:
                    r = slicebase(s, args[0], zero);
                    break;
                case Tyint:
                    args[0]->expr.type = n->expr.type;
                    r = args[0];
                    break;
                default:
                    fatal(n->line, "Bad cast from %s to %s",
                          tystr(exprtype(args[0])), tystr(exprtype(n)));
            }
            break;
        default:
            fatal(n->line, "Bad cast from %s to %s",
                  tystr(exprtype(args[0])), tystr(exprtype(n)));
    }
    return r;
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
            append(s, store(r, n));
        }
    }
    return r;
}

static Node *lowerucon(Simp *s, Node *n, Node *dst)
{
    Node *tmp, *u, *tag, *elt, *sz;
    Node *r;
    Type *ty;
    Ucon *uc;
    size_t i;

    /* find the ucon we're constructing here */
    ty = tybase(n->expr.type);
    for (i = 0; i < ty->nmemb; i++) {
        if (!strcmp(namestr(n->expr.args[0]), namestr(ty->udecls[i]->name))) {
            uc = ty->udecls[i];
            break;
        }
    }

    if (dst)
        tmp = dst;
    else
        tmp = temp(s, n);
    u = addr(tmp, exprtype(n));
    tag = word(n->line, uc->id);
    append(s, store(u, tag));
    if (!uc->etype)
        return tmp;

    elt = rval(s, n->expr.args[1], NULL);
    u = add(u, wordsz);
    if (tysize(uc->etype) > Wordsz) {
        elt = addr(elt, uc->etype);
        sz = word(n->line, tysize(uc->utype));
        r = mkexpr(n->line, Oblit, u, elt, sz, NULL);
    } else {
        r = store(u, elt);
    }
    append(s, r);
    return tmp;
}

static Node *rval(Simp *s, Node *n, Node *dst)
{
    Node *r; /* expression result */
    Node *t, *u, *v; /* temporary nodes */
    Node **args;
    size_t i;
    const Op fusedmap[] = {
        [Oaddeq]        = Oadd,
        [Osubeq]        = Osub,
        [Omuleq]        = Omul,
        [Odiveq]        = Odiv,
        [Omodeq]        = Omod,
        [Oboreq]        = Obor,
        [Obandeq]       = Oband,
        [Obxoreq]       = Obxor,
        [Obsleq]        = Obsl,
    };


    r = NULL;
    args = n->expr.args;
    switch (exprop(n)) {
        case Obad:
        case Olor: case Oland:
            r = temp(s, n);
            simplazy(s, n, r);
            break;
        case Osize:
            r = word(n->line, size(args[0]));
            break;
        case Oslice:
            r = lowerslice(s, n, dst);
            break;
        case Oidx:
            t = idxaddr(s, n);
            r = load(t);
            break;
        case Omemb:
            if (exprtype(args[0])->type == Tyslice) {
                assert(!strcmp(namestr(args[1]), "len"));
                r = slicelen(s, args[0]);
            } else if (exprtype(args[0])->type == Tyarray) {
                assert(!strcmp(namestr(args[1]), "len"));
                r = exprtype(args[0])->asize;
            } else {
                t = membaddr(s, n);
                r = load(t);
            }
            break;
        case Ocons:
            r = lowerucon(s, n, dst);
            break;
        case Ocast:
            /* slice -> ptr cast */
            r = lowercast(s, n);
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
            r = store(u, v);
            break;

        /* ++expr(x)
         *  => args[0] = args[0] + 1
         *     expr(x) */
        case Opreinc:
            t = lval(s, args[0]);
            r = store(t, add(t, one));
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Opredec:
            t = lval(s, args[0]);
            r = store(t, sub(t, one));
            lappend(&s->incqueue, &s->nqueue, t);
            break;

        /* expr(x++)
         *     =>
         *      expr
         *      x = x + 1
         */
        case Opostinc:
            r = lval(s, args[0]);
            t = store(r, add(one, r));
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Opostdec:
            r = lval(s, args[0]);
            t = store(r, sub(one, r));
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Olit:
            switch (args[0]->lit.littype) {
                case Lchr: case Lbool: case Lint:
                    r = n;
                    break;
                case Lstr: case Lseq: case Lflt:
                    r = bloblit(s, n);
                    break;
                case Lfunc:
                    die("Func lits not handled yet");
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
                u = word(n->line, size(args[0]));
                v = mkexpr(n->line, Oblit, s->ret, t, u, NULL);
                append(s, v);
            } else if (n->expr.nargs && n->expr.args[0]) {
                t = s->ret;
                t = store(t, rval(s, args[0], NULL));
                append(s, t);
            }
            jmp(s, s->endlbl);
            break;
        case Oasn:
            t = lval(s, args[0]);
            u = rval(s, args[1], t);

            /* if we stored the result into t, rval() should return that,
             * and we know our work is done. */
            if (u == t) {
                r = t;
            } else if (size(n) > Wordsz) {
                t = addr(t, exprtype(n));
                u = addr(u, exprtype(n));
                v = word(n->line, size(n));
                r = mkexpr(n->line, Oblit, t, u, v, NULL);
            } else {
              r = store(t, u);
            }
            break;
        case Ocall:
            if (exprtype(n)->type != Tyvoid && size(n) > Wordsz) {
                r = temp(s, n);
                linsert(&n->expr.args, &n->expr.nargs, 1, addr(r, exprtype(n)));
                for (i = 0; i < n->expr.nargs; i++)
                    n->expr.args[i] = rval(s, n->expr.args[i], NULL);
                append(s, n);
            } else {
                r = visit(s, n);
            }
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
    if (debug)
        printf("declare %s(%ld) at %zd\n", declname(n), n->decl.did, s->stksz);
    htput(s->locs, n, (void*)s->stksz);
}

static void declarearg(Simp *s, Node *n)
{
    assert(n->type == Ndecl);
    if (debug)
        printf("declare %s(%ld) at %zd\n", declname(n), n->decl.did, -(s->argsz + 8));
    htput(s->locs, n, (void*)-(s->argsz + 8));
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
        case Nifstmt:    simpif(s, n);          break;
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

static void reduce(Simp *s, Node *f)
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
    if (ty->type != Tyvoid && tysize(ty) > Wordsz) {
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

static Func *lowerfn(Simp *s, char *name, Node *n)
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
    reduce(s, n);

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
    fn->isglobl = 1; /* FIXME: we should actually use the visibility of the sym... */
    fn->stksz = s->stksz;
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

static void lowerdcl(Node *dcl, Htab *globls, Func ***fn, size_t *nfn, Node ***blob, size_t *nblob)
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
            f = lowerfn(&s, name, dcl->decl.init);
            lappend(fn, nfn, f);
        }
    } else {
        if (dcl->decl.init && exprop(dcl->decl.init) == Olit)
            lappend(&s.blobs, &s.nblobs, dcl);
        /* uninitialized global vars get zero-initialized decls */
        else if (!dcl->decl.isconst && !dcl->decl.init)
            lappend(&s.blobs, &s.nblobs, dcl);
        else
            die("We don't lower globls with nonlit inits yet...");
    }
    *blob = s.blobs;
    *nblob = s.nblobs;
    free(name);
}

void gen(Node *file, char *out)
{
    Htab *globls;
    Node **n, **blob;
    Func **fn;
    size_t nn, nfn, nblob;
    size_t i;
    FILE *fd;

    /* declare useful constants */
    tyword = mkty(-1, Tyint);
    tyvoid = mkty(-1, Tyvoid);
    one = word(-1, 1);
    zero = word(-1, 0);
    ptrsz = word(-1, Wordsz);
    wordsz = word(-1, Wordsz);

    fn = NULL;
    nfn = 0;
    blob = NULL;
    nblob = 0;
    n = file->file.stmts;
    nn = file->file.nstmts;
    globls = mkht(dclhash, dcleq);

    /* We need to define all global variables before use */
    fillglobls(file->file.globls, globls);

    for (i = 0; i < nn; i++) {
        switch (n[i]->type) {
            case Nuse: /* nothing to do */ 
                break;
            case Ndecl:
                lowerdcl(n[i], globls, &fn, &nfn, &blob, &nblob);
                break;
            default:
                die("Bad node %s in toplevel", nodestr(n[i]->type));
                break;
        }
    }

    fd = fopen(out, "w");
    if (!fd)
        die("Couldn't open fd %s", out);

    if (debug) {
        for (i = 0; i < nblob; i++)
            genblob(stdout, blob[i], globls);
        for (i = 0; i < nfn; i++)
            genasm(stdout, fn[i], globls);
    }
    fprintf(fd, ".data\n");
    for (i = 0; i < nblob; i++)
        genblob(fd, blob[i], globls);
    fprintf(fd, ".text\n");
    for (i = 0; i < nfn; i++)
        genasm(fd, fn[i], globls);
    fclose(fd);
}
