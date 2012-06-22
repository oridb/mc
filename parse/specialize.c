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

static ulong tidhash(void *p)
{
    Type *t;

    t = p;
    return ptrhash((void*)(intptr_t)t->tid);
}

static int tideq(void *pa, void *pb)
{
    Type *a, *b;

    a = pa;
    b = pb;
    return a->tid == b->tid;
}

static int hasparams(Type *t)
{
    size_t i;

    if (t->type == Typaram)
        return 1;
    for (i = 0; i < t->nsub; i++)
        if (hasparams(t->sub[i]))
            return 1;
    return 0;
}

static Type *dosubst(Type *t, Htab *tsmap)
{
    Type *ret;
    size_t i;

    if (t->type == Typaram)
        return htget(tsmap, t);

    ret = tydup(t);
    for (i = 0; i < t->nsub; i++)
        ret->sub[i] = dosubst(t->sub[i], tsmap);
    return ret;
}

static Type *tysubst(Type *t, Htab *tsmap)
{
    if (hasparams(t))
        return dosubst(t, tsmap);
    else
        return t;
}

static void fillsubst(Htab *tsmap, Type *to, Type *from)
{
    size_t i;

    printf("Specialize %s => %s\n", tystr(from), tystr(to));
    htput(tsmap, from, to);
    if (to->nsub != from->nsub)
        return;
    for (i = 0; i < to->nsub; i++)
        fillsubst(tsmap, to->sub[i], from->sub[i]);
}

static Node *specializenode(Node *n, Htab *tsmap)
{
    Node *r;
    size_t i;

    r = mknode(n->line, n->type);
    switch (n->type) {
        case Nfile:
        case Nuse:
            die("Node %s not allowed here\n", nodestr(n->type));
            break;
        case Nexpr:
            r->expr.op = n->expr.op;
            r->expr.type = tysubst(n->expr.type, tsmap);
            r->expr.isconst = n->expr.isconst;
            r->expr.nargs = n->expr.nargs;
            r->expr.args = xalloc(n->expr.nargs * sizeof(Node*));
            for (i = 0; i < n->expr.nargs; i++)
                r->expr.args[i] = specializenode(n->expr.args[i], tsmap);
            break;
        case Nname:
            if (n->name.ns)
                r->name.ns = strdup(n->name.ns);
            r->name.name = strdup(n->name.name);
            break;
        case Nlit:
            r->lit.littype = n->lit.littype;
            r->lit.type = tysubst(n->expr.type, tsmap);
            switch (n->lit.littype) {
                case Lchr:      n->lit.chrval = n->lit.chrval;       break;
                case Lint:      n->lit.intval = n->lit.intval;       break;
                case Lflt:      n->lit.fltval = n->lit.fltval;       break;
                case Lstr:      n->lit.strval = n->lit.strval;       break;
                case Lbool:     n->lit.boolval = n->lit.boolval;     break;
                case Lfunc:     n->lit.fnval = specializenode(n->lit.fnval, tsmap);       break;
                case Larray:    n->lit.arrval = specializenode(n->lit.arrval, tsmap);     break;
            }
            break;
        case Nloopstmt:
            r->loopstmt.init = specializenode(n->loopstmt.init, tsmap);
            r->loopstmt.cond = specializenode(n->loopstmt.cond, tsmap);
            r->loopstmt.step = specializenode(n->loopstmt.step, tsmap);
            r->loopstmt.body = specializenode(n->loopstmt.body, tsmap);
            break;
        case Nifstmt:
            r->ifstmt.cond = specializenode(n->ifstmt.cond, tsmap);
            r->ifstmt.iftrue = specializenode(n->ifstmt.iftrue, tsmap);
            r->ifstmt.iffalse = specializenode(n->ifstmt.iffalse, tsmap);
            break;
        case Nblock:
            r->block.scope = mkstab();
            r->block.nstmts = n->block.nstmts;
            r->block.stmts = xalloc(sizeof(Node *)*n->block.nstmts);
            for (i = 0; i < n->block.nstmts; i++)
                r->block.stmts[i] = specializenode(n->block.stmts[i], tsmap);
            break;
        case Nlbl:
            r->lbl.name = strdup(n->lbl.name);
            break;
        case Ndecl:
            /* sym */
            r->decl.name = specializenode(n->decl.name, tsmap);
            n->decl.type = tysubst(n->decl.type, tsmap);

            /* symflags */
            r->decl.isconst = n->decl.isconst;
            r->decl.isgeneric = n->decl.isgeneric;
            r->decl.isextern = n->decl.isextern;

            /* init */
            r->decl.init = specializenode(n->decl.init, tsmap);
            break;
        case Nfunc:
            r->func.scope = mkstab();
            r->func.nargs = n->func.nargs;
            r->func.args = xalloc(sizeof(Node *)*n->func.nargs);
            for (i = 0; i < n->func.nargs; i++)
                r->func.args[i] = specializenode(n->func.args[i], tsmap);
            r->func.body = specializenode(n->func.body, tsmap);
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
    return r;
}

Node *specialize(Node *n, Type *to, Type *from)
{
    Htab *tsmap;

    tsmap = mkht(tidhash, tideq);
    fillsubst(tsmap, to, from);
    return specializenode(n, tsmap);
}
