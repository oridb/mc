#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"

Node *mknode(int line, Ntype nt)
{
    Node *n;

    n = zalloc(sizeof(Node));
    n->type = nt;
    n->line = line;
    return n;
}

Node *mkfile(char *name)
{
    Node *n;

    n = mknode(-1, Nfile);
    n->file.name = strdup(name);
    return n;
}

Node *mkuse(int line, char *use, int islocal)
{
    Node *n;

    n = mknode(line, Nuse);
    n->use.name = strdup(use);
    n->use.islocal = islocal;

    return n;
}

Node *mkexpr(int line, Op op, ...)
{
    Node *n;
    va_list ap;
    Node *arg;

    n = mknode(line, Nexpr);
    n->expr.op = op;
    va_start(ap, op);
    while ((arg = va_arg(ap, Node*)) != NULL)
        lappend(&n->expr.args, &n->expr.nargs, arg);
    va_end(ap);

    return n;
}

Node *mkcall(int line, Node *fn, Node **args, size_t nargs) 
{
    Node *n;
    int i;

    n = mkexpr(line, Ocall, fn, NULL);
    for (i = 0; i < nargs; i++)
        lappend(&n->expr.args, &n->expr.nargs, args[i]);
    return n;
}

Node *mkif(int line, Node *cond, Node *iftrue, Node *iffalse)
{
    Node *n;

    n = mknode(line, Nifstmt);
    n->ifstmt.cond = cond;
    n->ifstmt.iftrue = iftrue;
    n->ifstmt.iffalse = iffalse;

    return n;
}

Node *mkloop(int line, Node *init, Node *cond, Node *incr, Node *body)
{
    Node *n;

    n = mknode(line, Nloopstmt);
    n->loopstmt.init = init;
    n->loopstmt.cond = init;
    n->loopstmt.step = incr;
    n->loopstmt.body = body;

    return n;
}

Node *mkfunc(int line, Node **args, size_t nargs, Type *ret, Node *body)
{
    Node *n;
    Node *f;
    int i;

    f = mknode(line, Nfunc);
    f->func.args = args;
    f->func.nargs = nargs;
    f->func.body = body;
    f->func.scope = mkstab();
    f->func.type = mktyfunc(line, args, nargs, ret);

    for (i = 0; i < nargs; i++)
        putdcl(f->func.scope, args[i]->decl.sym);

    n = mknode(line, Nlit);
    n->lit.littype = Lfunc;
    n->lit.fnval = f;
    return n;
}

Node *mkblock(int line, Stab *scope)
{
    Node *n;

    n = mknode(line, Nblock);
    n->block.scope = scope;
    return n;
}

Node *mklabel(int line, char *lbl)
{
    Node *n;

    n = mknode(line, Nlbl);
    n->lbl.name = strdup(lbl);
    return n;
}

Node *mkstr(int line, char *val)
{
    Node *n;

    n = mknode(line, Nlit);
    n->lit.littype = Lstr;
    n->lit.strval = strdup(val);

    return n;
}

Node *mkint(int line, uint64_t val)
{
    Node *n;

    n = mknode(line, Nlit);
    n->lit.littype = Lint;
    n->lit.intval = val;

    return n;
}

Node *mkchar(int line, uint32_t val)
{
    Node *n;

    n = mknode(line, Nlit);
    n->lit.littype = Lchr;
    n->lit.chrval = val;

    return n;
}

Node *mkfloat(int line, double val)
{
    Node *n;

    n = mknode(line, Nlit);
    n->lit.littype = Lflt;
    n->lit.fltval = val;

    return n;
}

Node *mkname(int line, char *name)
{
    Node *n;

    n = mknode(line, Nname);
    n->name.nparts = 1;
    n->name.parts = xalloc(sizeof(char*));
    n->name.parts[0] = strdup(name);

    return n;
}

Node *mkdecl(int line, Sym *sym)
{
    Node *n;

    n = mknode(line, Ndecl);
    n->decl.sym = sym;
    return n;
}

Type *decltype(Node *n)
{
    assert(n->type == Ndecl);
    return n->decl.sym->type;
}

void setns(Node *n, char *name)
{
    int i;

    n->name.nparts++;
    n->name.parts = xrealloc(n->name.parts, n->name.nparts);
    for (i = n->name.nparts - 1; i > 0; i++)
        n->name.parts[i] = n->name.parts[i-1];
    n->name.parts[0] = strdup(name);
}

Node *mkbool(int line, int val)
{
    Node *n;

    n = mknode(line, Nlit);
    n->lit.littype = Lbool;
    n->lit.boolval = val;

    return n;
}

void lappend(void *l, size_t *len, void *n)
{
    void ***pl;

    assert(n != NULL);
    pl = l;
    *pl = xrealloc(*pl, (*len + 1)*sizeof(Node*));
    (*pl)[*len] = n;
    (*len)++;
}

