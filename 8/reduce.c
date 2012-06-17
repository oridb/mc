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
    Node *retval;

    /* pre/postinc handling */
    Node **incqueue;
    size_t nqueue;

    /* location handling */
    size_t stksz;
    size_t argsz;
    Htab *locs;
    Htab *globls;
    Node *ret;
};

static Node *simp(Simp *s, Node *n);
static Node *rval(Simp *s, Node *n);
static Node *lval(Simp *s, Node *n);
static void declarelocal(Simp *s, Node *n);

/* useful constants */
static Node *one;
static Node *ptrsz;

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
    switch (t->type) {
        case Tyvoid:
            return 1;
        case Tybool: case Tychar: case Tyint8:
        case Tybyte: case Tyuint8:
            return 1;
        case Tyint16: case Tyuint16:
            return 2;
        case Tyint: case Tyint32:
        case Tyuint: case Tyuint32:
        case Typtr: case Tyenum:
        case Tyfunc:
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
            die("Sizes for composite types not implemented yet");
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

static Node *genlbl(void)
{
    char buf[128];
    static int nextlbl;

    snprintf(buf, 128, ".L%d", nextlbl++);
    return mklbl(-1, buf);
}

static Node *temp(Simp *simp, Node *e)
{
    char buf[128];
    static int nexttmp;
    Node *t, *r, *n;

    assert(e->type == Nexpr);
    snprintf(buf, 128, ".t%d", nexttmp++);
    n = mkname(e->line, buf);
    t = mkdecl(e->line, n, e->expr.type);
    declarelocal(simp, t);
    r = mkexpr(e->line, Ovar, t, NULL);
    r->expr.did = t->decl.did;
    return r;
}

static void jmp(Simp *s, Node *lbl)
{
    append(s, mkexpr(lbl->line, Ojmp, lbl, NULL));
}

static Node *store(Node *dst, Node *src)
{
    return mkexpr(dst->line, Ostor, dst, src, NULL);
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
    c = rval(s, n->ifstmt.cond);
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
    t = rval(s, n->loopstmt.cond);  /* test */
    cjmp(s, t, lbody, lend);    /* repeat? */
    simp(s, lend);              /* exit */
}

static void simpblk(Simp *s, Node *n)
{
    size_t i;

    for (i = 0; i < n->block.nstmts; i++) {
        simp(s, n->block.stmts[i]);
    }
}

static size_t offsetof(Node *aggr, Node *memb)
{
    Type *ty;
    Node **nl;
    size_t nn, i;
    size_t off;

    if (aggr->expr.type->type == Typtr)
        aggr = aggr->expr.args[0];
    ty = aggr->expr.type;

    assert(ty->type == Tystruct);
    nl = aggrmemb(ty, &nn);
    off = 0;
    for (i = 0; i < nn; i++) {
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
    if (n->expr.type->type != Typtr)
        t = mkexpr(n->line, Oaddr, args[0], NULL);
    else
        t = args[0];
    u = mkintlit(n->line, offsetof(args[0], args[1]));
    r = mkexpr(n->line, Oadd, t, u, NULL);
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
    if (args[0]->expr.type->type == Tyarray)
        t = mkexpr(n->line, Oaddr, args[0], NULL);
    else if (args[0]->expr.type->type == Tyslice)
        t = mkexpr(n->line, Oload, mkexpr(n->line, Oaddr, args[0], NULL), NULL);
    else
        die("Can't index type %s\n", tystr(n->expr.type));
    u = rval(s, args[1]);
    sz = size(n);
    v = mkexpr(n->line, Omul, u, mkintlit(n->line, sz), NULL);
    r = mkexpr(n->line, Oadd, t, v, NULL);
    return r;
}

static Node *slicebase(Simp *s, Node *n, Node *off)
{
    Node *t, *u, *v;
    int sz;

    t = rval(s, n);
    u = NULL;
    switch (n->expr.type->type) {
        case Typtr:     u = n;
        case Tyarray:   u = mkexpr(n->line, Oaddr, t, NULL); break;
        case Tyslice:   u = mkexpr(n->line, Oslbase, t, NULL); break;
        default: die("Unslicable type %s", tystr(n->expr.type));
    }
    /* safe: all types we allow here have a sub[0] that we want to grab */
    sz = tysize(n->expr.type->sub[0]);
    v = mkexpr(n->line, Omul, off, mkintlit(n->line, sz), NULL);
    return mkexpr(n->line, Oadd, u, v, NULL);
}

static Node *slicelen(Simp *s, Node *sl)
{
    Node *base, *memb, *load;

    base = mkexpr(sl->line, Oaddr, sl, NULL);
    memb = mkexpr(sl->line, Oadd, base, ptrsz, NULL);
    load = mkexpr(sl->line, Oload, memb, NULL);
    return load;
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
    a = rval(s, n->expr.args[0]);
    append(s, store(r, a));
    if (exprop(n) == Oland)
        cjmp(s, a, next, end);
    else if (exprop(n) == Olor)
        cjmp(s, a, end, next);
    append(s, next);
    b = rval(s, n->expr.args[1]);
    append(s, store(r, b));
    append(s, end);
    return r;
}

static Node *lowerslice(Simp *s, Node *n)
{
    Node *t;
    Node *base, *sz, *len;
    Node *stbase, *stlen;

    t = temp(s, n);
    /* *(&slice) = (void*)base + off*sz */
    base = slicebase(s, n->expr.args[0], n->expr.args[1]);
    len = mkexpr(n->line, Osub, n->expr.args[2], n->expr.args[1], NULL);
    stbase = mkexpr(n->line, Ostor, mkexpr(n->line, Oaddr, t, NULL), base, NULL);
    /* *(&slice + ptrsz) = len */
    sz = mkexpr(n->line, Oadd, mkexpr(n->line, Oaddr, t, NULL), ptrsz, NULL);
    stlen = mkexpr(n->line, Ostor, sz, len, NULL);
    append(s, stbase);
    append(s, stlen);
    return t;
}

static Node *rval(Simp *s, Node *n)
{
    Node *r; /* expression result */
    Node *t, *u, *v; /* temporary nodes */
    size_t i;
    Node **args;
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


    if (!one)
        one = mkintlit(-1, 1);
    r = NULL;
    args = n->expr.args;
    switch (exprop(n)) {
        case Obad:
        case Olor: case Oland:
            r = temp(s, n);
            simplazy(s, n, r);
            break;
        case Osize:
            r = mkintlit(n->line, size(args[0]));
            break;
        case Oslice:
            r = lowerslice(s, n);
            break;
        case Oidx:
            t = idxaddr(s, n);
            r = mkexpr(n->line, Oload, t, NULL);
            break;
        case Omemb:
            if (exprtype(args[0])->type == Tyslice) {
                assert(!strcmp(namestr(args[1]), "len"));
                r = slicelen(s, args[0]);
            } else if (exprtype(args[0])->type == Tyarray) {
                assert(!strcmp(namestr(args[1]), "len"));
                r = exprtype(n)->asize;
            } else {
                t = membaddr(s, n);
                r = mkexpr(n->line, Oload, t, NULL);
            }
            break;

        /* fused ops:
         * foo ?= blah
         *    =>
         *     foo = foo ? blah*/
        case Oaddeq: case Osubeq: case Omuleq: case Odiveq: case Omodeq:
        case Oboreq: case Obandeq: case Obxoreq: case Obsleq: case Obsreq:
            assert(fusedmap[exprop(n)] != Obad);
            u = rval(s, args[0]);
            v = rval(s, args[1]);
            v = mkexpr(n->line, fusedmap[exprop(n)], u, v, NULL);
            r = mkexpr(n->line, Ostor, u, v, NULL);
            break;

        /* ++expr(x)
         *  => args[0] = args[0] + 1
         *     expr(x) */
        case Opreinc:
            t = lval(s, args[0]);
            v = mkexpr(n->line, Oadd, one, t, NULL);
            r = mkexpr(n->line, Ostor, t, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Opredec:
            t = lval(s, args[0]);
            v = mkexpr(n->line, Osub, one, t, NULL);
            r = mkexpr(n->line, Ostor, t, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t);
            break;

        /* expr(x++)
         *     =>
         *      expr
         *      x = x + 1
         */
        case Opostinc:
            r = lval(s, args[0]);
            v = mkexpr(n->line, Oadd, one, r, NULL);
            t = mkexpr(n->line, Ostor, r, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Opostdec:
            r = lval(s, args[0]);
            v = mkexpr(n->line, Osub, one, args[0], NULL);
            t = mkexpr(n->line, Ostor, r, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t);
            break;
        case Olit: case Ovar:
            r = n;
            break;
        case Oret:
            if (n->expr.args[0]) {
                if (s->ret)
                    t = s->ret;
                else
                    t = s->ret = temp(s, args[0]);
                t = store(t, rval(s, args[0]));
                append(s, t);
            }
            jmp(s, s->endlbl);
            break;
        case Oasn:
            t = lval(s, args[0]);
            u = rval(s, args[1]);
            if (size(n) > 4) {
                t = mkexpr(n->line, Oaddr, t, NULL);
                u = mkexpr(n->line, Oaddr, u, NULL);
		v = mkintlit(n->line, size(n));
                r = mkexpr(n->line, Oblit, t, u, v, NULL);
            } else {
              r = mkexpr(n->line, Ostor, t, u, NULL);
            }
            break;
        default:
            for (i = 0; i < n->expr.nargs; i++)
                n->expr.args[i] = rval(s, n->expr.args[i]);
            if (ispure(n)) {
                r = n;
            } else {
                r = temp(s, n);
                append(s, store(r, n));
            }
    }
    return r;
}

static void declarelocal(Simp *s, Node *n)
{
    assert(n->type == Ndecl);
    s->stksz += size(n);
    if (debug)
        printf("DECLARE %s(%ld) at %zd\n", declname(n), n->decl.did, s->stksz);
    htput(s->locs, n, (void*)s->stksz);
}

static void declarearg(Simp *s, Node *n)
{
    assert(n->type == Ndecl);
    if (debug)
        printf("DECLARE %s(%ld) at %zd\n", declname(n), n->decl.did, -(s->argsz + 8));
    htput(s->locs, n, (void*)-(s->argsz + 8));
    s->argsz += size(n);
}

static Node *simp(Simp *s, Node *n)
{
    Node *r, *v;
    size_t i;

    if (!n)
        return NULL;
    r = NULL;
    switch (n->type) {
        case Nblock:
            simpblk(s, n);
            break;
        case Nifstmt:
            simpif(s, n);
            break;
        case Nloopstmt:
            simploop(s, n);
            break;
        case Nexpr:
            r = rval(s, n);
            if (r)
                append(s, r);
            /* drain the increment queue for this expr */
            for (i = 0; i < s->nqueue; i++)
                append(s, s->incqueue[i]);
            lfree(&s->incqueue, &s->nqueue);
            break;
        case Nlit:
            r = n;
            break;
        case Ndecl:
            /* Should already have happened */
	    declarelocal(s, n);
	    if (n->decl.init) {
		v = rval(s, n->decl.init);
		r = mkexpr(n->line, Ovar, n, NULL);
		r->expr.did = n->decl.did;
		append(s, store(r, v));
	    }
            break;
        case Nlbl:
            append(s, n);
            break;
        default:
            die("Bad node passsed to simp()");
            break;
    }
    return r;
}

static void reduce(Simp *s, Node *f)
{
    size_t i;

    assert(f->type == Nfunc);
    s->nstmts = 0;
    s->stmts = NULL;
    s->endlbl = genlbl();
    s->retval = NULL;

    if (f->type == Nfunc) {
        for (i = 0; i < f->func.nargs; i++) {
            declarearg(s, f->func.args[i]);
        }
        simp(s, f->func.body);
    } else {
        die("Got a non-func (%s) to reduce", nodestr(f->type));
    }

    append(s, s->endlbl);
}

static Func *lowerfn(Simp *s, char *name, Node *n)
{
    size_t i;
    Func *fn;
    Cfg *cfg;

    if(debug)
        printf("\n\nfunction %s\n", name);

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
	if (debug) {
	    printf("FOLD FROM ----------\n");
	    dump(s->stmts[i], stdout);
	}
	s->stmts[i] = fold(s->stmts[i]);
	if (debug) {
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

void blobdump(Blob *b, FILE *fd)
{
    size_t i;
    char *p;

    p = b->data;
    for (i = 0; i < b->ndata; i++)
        if (isprint(p[i]))
            fprintf(fd, "%c", p[i]);
        else
            fprintf(fd, "\\%x", p[i]);
    fprintf(fd, "\n");
}

void fillglobls(Stab *st, Htab *globls)
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

void lowerdcl(Node *dcl, Htab *globls, Func ***fn, size_t *nfn, Node ***blob, size_t *nblob)
{
    Simp s = {0,};
    char *name;
    Func *f;

    name = asmname(dcl->decl.name);
    s.locs = mkht(dclhash, dcleq);
    s.globls = globls;

    if (isconstfn(dcl)) {
        f = lowerfn(&s, name, dcl->decl.init);
        lappend(fn, nfn, f);
    } else {
        die("We don't lower globls yet...");
    }
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
    one = mkintlit(-1, 1);
    ptrsz = mkintlit(-1, 4);

    fn = NULL;
    nfn = 0;
    blob = NULL;
    nblob = 0;
    n = file->file.stmts;
    nn = file->file.nstmts;
    globls = mkht(dclhash, dcleq);

    /* We need to define all global variables before use */
    fillglobls(file->file.globls, globls);

    fd = fopen(out, "w");
    if (!fd)
        die("Couldn't open fd %s", out);
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

    for (i = 0; i < nfn; i++)
        genasm(fd, fn[i], globls);
    fclose(fd);
}
