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

static Node *lowerexpr(Comp *c, Fn *fn, Node *n);
static void label(Comp *c, Fn *fn, Node *lbl);
static void lowerlocal(Comp *c, Fn *fn, Node *init);
static void lowerglobl(Comp *c, char *name, Node *init);
static void lowerif(Comp *c, Fn *fn, Node *cond);
static void lowerloop(Comp *c, Fn *fn, Node *loop);
static void lowerblock(Comp *c, Fn *fn, Node *blk);
static void lowerfn(Comp *c, char *name, Node *n);

Fn *mkfn(char *name)
{
    Fn *f;

    f = zalloc(sizeof(Fn));
    f->name = strdup(name);

    f->start = mkbb();
    f->cur = mkbb();
    f->end = mkbb();

    edge(f->start, f->cur);
    edge(f->cur, f->end);

    return f;
}

static char *asmname(Node *n)
{
    int i;
    char *s;
    char *sep;
    int len;

    for (i = 0; i < n->name.nparts; i++)
        len += strlen(n->name.parts[i]) + 1;

    s = xalloc(len);
    s[0] = '\0';
    sep = "";
    for (i = 0; i < n->name.nparts; i++) {
        sprintf(s, "%s%s", sep, n->name.parts[i]);
        sep = "$";
    }
    return s;
}


Bb *mkbb()
{
    static int bbid;
    Bb *bb;

    bb = zalloc(sizeof(Bb));
    bb->id = bbid++;

    return bb;
}

void edge(Bb *from, Bb *to)
{
    lappend(&from->out, &from->nout, to);
    lappend(&to->in, &to->nin, to);
}

static Node *genlbl()
{
    static int lblid;
    static char buf[128];

    /* generate a local label */
    snprintf(buf, 128, ".L%d", lblid++);
    return mklbl(-1, strdup(buf));
}

/* support functions */
static void jmp(Comp *c, Fn *fn, Node *targ)
{
}

/* support functions */
static void cjmp(Comp *c, Fn *fn, Node *cond, Node *iftrue, Node *iffalse)
{
}

static void lowerglobl(Comp *c, char *name, Node *init)
{
    printf("gen globl %s\n", name);
}

static void lowerlocal(Comp *c, Fn *fn, Node *dcl)
{
    printf("gen local");
}

static Node *lowerexpr(Comp *c, Fn *fn, Node *n)
{
    lappend(&fn->cur->n, &fn->cur->nn, n);
    return n;
}

static void lowerif(Comp *c, Fn *fn, Node *cond)
{
    lowerexpr(c, fn, cond->ifstmt.cond);
    label(c, fn, genlbl());
    lowerexpr(c, fn, cond->ifstmt.iftrue);
    label(c, fn, genlbl());
    lowerexpr(c, fn, cond->ifstmt.iffalse);
}

static void lowerloop(Comp *c, Fn *fn, Node *loop)
{
    Node *start;
    Node *test;
    Node *end;
    Node *cond;

    /* structure of loop:
          init
          jmp test (if for/while, otherwise, fall through)
        start:
          body
          inc
        test:
          test
          cjmp cond start end
        end:
    */
    start = genlbl();
    test = genlbl();
    end = genlbl();

    jmp(c, fn, start);
    /* init */
    if (loop->loopstmt.init->type == Ndecl)
        lowerlocal(c, fn, loop->loopstmt.init);
    else
        lowerexpr(c, fn, loop->loopstmt.init);
    label(c, fn, start);

    /* body and inc */
    lowerexpr(c, fn, loop->loopstmt.body);
    lowerexpr(c, fn, loop->loopstmt.step);

    /* test */
    label(c, fn, test);
    cond = lowerexpr(c, fn, loop->loopstmt.cond);
    cjmp(c, fn, cond, start, end);
    label(c, fn, end);
}

static void label(Comp *c, Fn *fn, Node *lbl)
{
    lappend(&fn->fixup, &fn->nfixup, fn->cur);
    fn->cur = mkbb();
}

static void lowerblock(Comp *c, Fn *fn, Node *blk)
{
    Node **n;
    size_t nn;
    int i;

    assert(blk && blk->type == Nblock);
    n = blk->block.stmts;
    nn = blk->block.nstmts;
    for (i = 0; i < nn; i++) {
        switch (n[i]->type) {
            case Ndecl:
                lowerlocal(c, fn, n[i]);
                break;
            case Nexpr:
                lowerexpr(c, fn, n[i]);
                break;
            case Nifstmt:
                lowerif(c, fn, n[i]);
                break;
            case Nloopstmt:
                lowerloop(c, fn, n[i]);
                break;
            case Nlbl:
                label(c, fn, n[i]);
                break;
            default:
                die("Bad node %s in block", nodestr(n[i]->type));
                break;
        }
    }
}

static void lowerfn(Comp *c, char *name, Node *n)
{
    Fn *fn;
    int i;
    Bb *fix;
    Node *last;

    /* unwrap to the function body */
    assert(n->type == Nexpr && exprop(n) == Olit);
    n = n->expr.args[0];
    assert(n->type == Nlit && n->lit.littype == Lfunc);
    n = n->lit.fnval;
    assert(n->type == Nfunc);

    /* lower */
    fn = mkfn(name);
    fn = zalloc(sizeof(Fn));
    fn->name = strdup(name);
    lowerblock(c, fn, n->func.body);
    lappend(&c->func, &c->nfunc, fn);

    for (i = 0; i < fn->nfixup; i++) {
        fix = fn->fixup[i];
        last = fix->n[fix->nn - 1];
        switch (exprop(last)) {
            case Ocjmp:
                break;
            case Ojmp:
                break;
            default:
                die("Invalid last node in BB");
                break;
        }
    }
}

int isconstfn(Sym *s)
{
    return s->isconst && s->type->type == Tyfunc;
}

void gen(Node *file, char *out)
{
    Node **n;
    int nn, i;
    Sym *s;
    char *name;
    Comp *c;

    c = zalloc(sizeof(Comp));

    n = file->file.stmts;
    nn = file->file.nstmts;

    for (i = 0; i < nn; i++) {
        switch (n[i]->type) {
            case Nuse: /* nothing to do */ 
                break;
            case Ndecl:
                s = n[i]->decl.sym;
                name = asmname(s->name);
                if (isconstfn(s)) {
                    lowerfn(c, name, n[i]->decl.init);
                    free(name);
                } else {
                    lowerglobl(c, name, n[i]);
                }
                break;
            default:
                die("Bad node %s in toplevel", nodestr(n[i]->type));
                break;
        }
    }

    /*
    isel();
    regalloc();
    */
}
