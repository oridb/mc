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
#include "gen.h"
#include "asm.h"

void breakhere()
{
    volatile int x = 0;
    x++;
}

/* takes a list of nodes, and reduces it (and it's subnodes) to a list
 * following these constraints:
 *      - All nodes are expression nodes
 *      - Nodes with side effects are root nodes
 *      - All nodes operate on machine-primitive types and tuples
 */
typedef struct Simp Simp;
struct Simp {
    Node **blk;
    size_t nblk;

    /* the function that we're reducing the body for */
    Fn *fn;

    /* return handling */
    Node *endlbl;
    Node *retval;

    /* pre/postinc handling */
    Node **incqueue;
    size_t nqueue;
};

Node *simp(Simp *s, Node *n);
Node *rval(Simp *s, Node *n);
Node *lval(Simp *s, Node *n);
void declare(Simp *s, Node *n);

void append(Simp *s, Node *n)
{
    lappend(&s->blk, &s->nblk, n);
}

int isimpure(Node *n)
{
    return 0;
}

size_t size(Node *n)
{
    Type *t;

    if (n->type == Nexpr)
        t = n->expr.type;
    else
        t = n->decl.sym->type;

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
        case Tytuple:
        case Tystruct:
        case Tyunion:
            die("Sizes for composite types not implemented yet");
            break;
        case Tybad:
        case Tyvar:
        case Typaram:
        case Tyname:
        case Ntypes:
            die("Type %s does not have size; why did it get down to here?", tystr(t));
            break;
    }
    return -1;
}

Node *genlbl(void)
{
    char buf[128];
    static int nextlbl;

    snprintf(buf, 128, ".L%d", nextlbl++);
    return mklbl(-1, buf);
}

Node *temp(Simp *simp, Node *e)
{
    char buf[128];
    static int nexttmp;
    Node *n;
    Node *t;
    Sym *s;

    assert(e->type == Nexpr);
    snprintf(buf, 128, ".t%d", nexttmp++);
    n = mkname(-1, buf);
    s = mksym(-1, n, e->expr.type);
    t = mkdecl(-1, s);
    declare(simp, t);
    return mkexpr(-1, Ovar, t, NULL);
}

void jmp(Simp *s, Node *lbl) { append(s, mkexpr(-1, Ojmp, lbl, NULL)); }
Node *store(Node *t, Node *n) { return mkexpr(-1, Ostor, t, n, NULL); }
Node *storetmp(Simp *s, Node *n) { return store(temp(s, n), n); }

void cjmp(Simp *s, Node *cond, Node *iftrue, Node *iffalse)
{
    Node *jmp;

    jmp = mkexpr(-1, Ocjmp, cond, iftrue, iffalse, NULL);
    append(s, jmp);
}

/* if foo; bar; else baz;;
 *      => cjmp (foo) :bar :baz */
void simpif(Simp *s, Node *n)
{
    Node *l1, *l2;
    Node *c;

    l1 = genlbl();
    l2 = genlbl();
    c = simp(s, n->ifstmt.cond);
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
void simploop(Simp *s, Node *n)
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

void simpblk(Simp *s, Node *n)
{
    int i;

    for (i = 0; i < n->block.nstmts; i++) {
        simp(s, n->block.stmts[i]);
    }
}

Node *lval(Simp *s, Node *n)
{
    return rval(s, n);
}

static Node *one;

Node *rval(Simp *s, Node *n)
{
    Node *r, *t, *u, *v;
    int i;
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
        one = mkexpr(-1, Olit, mkint(-1, 1), NULL);
    r = NULL;
    args = n->expr.args;
    switch (exprop(n)) {
        case Obad: 
        case Olor: case Oland:
        case Obsreq: case Omemb:
        case Oslice: case Oidx: case Osize:
            die("Have not implemented lowering op %s", opstr(exprop(n)));
            break;

        /* fused ops:
         * foo ?= blah
         *    =>
         *     foo = foo ? blah*/
        case Oaddeq: case Osubeq: case Omuleq: case Odiveq: case Omodeq:
        case Oboreq: case Obandeq: case Obxoreq: case Obsleq:
            assert(fusedmap[exprop(n)] != Obad);
            u = rval(s, args[0]);
            v = rval(s, args[1]);
            v = mkexpr(-1, fusedmap[exprop(n)], u, v, NULL);
            r = mkexpr(-1, Ostor, u, v, NULL);
            break;

        /* ++expr(x)
         *  => args[0] = args[0] + 1
         *     expr(x) */
        case Opreinc:
            t = lval(s, args[0]);
            v = mkexpr(-1, Oadd, one, t, NULL);
            r = mkexpr(-1, Ostor, t, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;
        case Opredec:
            t = lval(s, args[0]);
            v = mkexpr(-1, Osub, one, t, NULL);
            r = mkexpr(-1, Ostor, t, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;

        /* expr(x++)
         *     => 
         *      expr
         *      x = x + 1 
         */
        case Opostinc:
            r = lval(s, args[0]);
            v = mkexpr(-1, Oadd, one, r, NULL);
            t = mkexpr(-1, Ostor, r, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;
        case Opostdec:
            r = lval(s, args[0]);
            v = mkexpr(-1, Osub, one, args[0], NULL);
            t = mkexpr(-1, Ostor, r, v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;
        case Olit:
        case Ovar:
            r = n;
            break;
        case Oret:
            if (n->expr.args[0]) {
                if (s->fn->ret)
                    t = s->fn->ret;
                else
                    t = s->fn->ret = temp(s, args[0]);
                t = store(t, rval(s, args[0]));
                append(s, t);
            }
            jmp(s, s->endlbl);
            break;
        case Oasn:
            t = lval(s, args[0]);
            v = rval(s, args[1]);
            r = mkexpr(-1, Ostor, t, v, NULL);
            break;
        default:
            if (isimpure(n)) {
                v = rval(s, n);
                t = storetmp(s, v);
                append(s, t);
                r = t;
            } else {
                for (i = 0; i < n->expr.nargs; i++)
                    n->expr.args[i] = rval(s, n->expr.args[i]);
                r = n;
            }
    }
    return r;
}

void declare(Simp *s, Node *n)
{
    Fn *f;

    assert(n->type == Ndecl);
    f = s->fn;
    htput(f->locs, (void*)n->decl.sym->id, (void*)f->stksz);
    f->stksz += size(n);
}

Node *simp(Simp *s, Node *n)
{
    Node *r;
    int i;

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
            free(s->incqueue);
            s->nqueue = 0;
            s->incqueue = NULL;
            break;
        case Nlit:
            r = n;
            break;
        case Ndecl:
            declare(s, n);
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

Node **reduce(Fn *fn, Node *n, int *ret_nn)
{
    Simp s = {0,};

    s.nblk = 0;
    s.endlbl = genlbl();
    s.retval = NULL;
    s.fn = fn;

    if (n->type == Nblock)
        simp(&s, n);
    else
        die("Got a non-block (%s) to reduce", nodestr(n->type));

    append(&s, s.endlbl);

    *ret_nn = s.nblk;
    return s.blk;
}
