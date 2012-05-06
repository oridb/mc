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

    /* return handling */
    Node *endlbl;
    Node *retval;

    /* pre/postinc handling */
    Node **incqueue;
    size_t nqueue;
};

Node *simp(Simp *s, Node *n);

void append(Simp *s, Node *n)
{
    lappend(&s->blk, &s->nblk, n);
}

int isimpure(Node *n)
{
    return 0;
}

Node *genlbl(void)
{
    char buf[128];
    static int nextlbl;

    snprintf(buf, 128, ".L%d", nextlbl++);
    return mklbl(-1, buf);
}

Node *temp(Node *e)
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
    return mkexpr(-1, Ovar, t, NULL);
}

void cjmp(Simp *s, Node *cond, Node *iftrue, Node *iffalse)
{
    Node *jmp;

    jmp = mkexpr(-1, Ocjmp, cond, iftrue, iffalse, NULL);
    append(s, jmp);
}

void jmp(Simp *s, Node *lbl)
{
    append(s, mkexpr(-1, Ojmp, lbl, NULL));
}

Node *store(Node *t, Node *n)
{
    return mkexpr(-1, Oasn, t, n, NULL);
}

Node *storetmp(Node *n)
{
    return store(temp(n), n);
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
    Node *c;

    lbody = genlbl();
    lcond = genlbl();
    lend = genlbl();

    simp(s, n->loopstmt.init);
    jmp(s, lcond);
    simp(s, lbody);
    simp(s, n->loopstmt.body);
    simp(s, n->loopstmt.step);
    simp(s, lcond);
    c = simp(s, n->loopstmt.cond);
    cjmp(s, c, lbody, lend);
    simp(s, lend);
}

void simpblk(Simp *s, Node *n)
{
    int i;
    Node *e;

    for (i = 0; i < n->block.nstmts; i++) {
        e = simp(s, n->block.stmts[i]);
        if (e)
            append(s, e);
    }
}

Node *simpexpr(Simp *s, Node *n)
{
    Node *r, *t, *v;
    int i;
    Node **args;
    const Op fusedmap[] = {
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
        case Olor: case Oland: case Oaddeq:
        case Obsreq: case Omemb:
        case Oslice: case Oidx: case Osize:
            die("Have not implemented lowering op %s", opstr(exprop(n)));
            break;

        /* fused ops:
         * foo ?= blah
         *    =>
         *     foo = foo ? blah*/
        case Osubeq: case Omuleq: case Odiveq: case Omodeq: case Oboreq:
        case Obandeq: case Obxoreq: case Obsleq:
            v = mkexpr(-1, fusedmap[exprop(n)], args[0], args[1], NULL);
            r = mkexpr(-1, Oasn, args[0], v, NULL);
            break;

        /* ++expr(x)
         *  => x = x + 1
         *     expr(x) */
        case Opreinc:
            t = simp(s, args[0]);
            v = mkexpr(-1, Oadd, mkint(-1, 1), args[0], NULL);
            r = mkexpr(-1, Oasn, args[0], v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;
        case Opredec:
            t = simp(s, args[0]);
            v = mkexpr(-1, Oadd, mkint(-1, -1), args[0], NULL);
            r = mkexpr(-1, Oasn, args[0], v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;

        /* expr(x++)
         *     => 
         *      expr
         *      x = x + 1 
         */
        case Opostinc:
            r = simp(s, args[0]);
            v = mkexpr(-1, Oadd, mkint(-1, 1), r, NULL);
            t = mkexpr(-1, Oasn, args[0], v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;
        case Opostdec:
            r = simp(s, args[0]);
            v = mkexpr(-1, Osub, mkint(-1, -1), args[0], NULL);
            t = mkexpr(-1, Oasn, args[0], v, NULL);
            lappend(&s->incqueue, &s->nqueue, t); 
            break;
        case Ovar:
            r = n;
            break;
        case Oret:
            if (n->expr.args[0]) {
                t = storetmp(simp(s, n->expr.args[0]));
                append(s, t);
            }
            jmp(s, s->endlbl);
            break;
        default:
            if (isimpure(n)) {
                v = simp(s, n);
                t = storetmp(v);
                append(s, t);
                r = t;
            } else {
                for (i = 0; i < n->expr.nargs; i++)
                    n->expr.args[i] = simp(s, n->expr.args[i]);
                r = n;
            }
    }
    return r;
}

Node *simp(Simp *s, Node *n)
{
    Node *r;

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
            r = simpexpr(s, n);
            break;
        case Nlit:
            r = n;
            break;
        case Ndecl:
            //declare(s, n);
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

Node **reduce(Node *n, int *ret_nn)
{
    Simp s = {0,};

    s.nblk = 0;
    s.endlbl = genlbl();
    s.retval = NULL;
    if (n->type == Nblock)
        simp(&s, n);
    else
        die("Got a non-block (%s) to reduce", nodestr(n->type));

    append(&s, s.endlbl);

    *ret_nn = s.nblk;
    return s.blk;
}
