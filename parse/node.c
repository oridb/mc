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

size_t maxnid;
Node **decls;
size_t ndecls;

Node *mknode(int line, Ntype nt)
{
    Node *n;

    n = zalloc(sizeof(Node));
    n->nid = maxnid++;
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

Node *mksliceexpr(int line, Node *sl, Node *base, Node *off)
{
    if (!base)
        base = mkintlit(line, 0);
    if (!off)
        off = mkexpr(line, Omemb, sl, mkname(line, "len"), NULL);
    return mkexpr(line, Oslice, sl, base, off, NULL);
}

Node *mkexprl(int line, Op op, Node **args, size_t nargs)
{
    Node *n;

    n = mknode(line, Nexpr);
    n->expr.op = op;
    n->expr.args = args;
    n->expr.nargs = nargs;
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
    size_t i;

    n = mkexpr(line, Ocall, fn, NULL);
    for (i = 0; i < nargs; i++)
        lappend(&n->expr.args, &n->expr.nargs, args[i]);
    return n;
}

Node *mkifstmt(int line, Node *cond, Node *iftrue, Node *iffalse)
{
    Node *n;

    n = mknode(line, Nifstmt);
    n->ifstmt.cond = cond;
    n->ifstmt.iftrue = iftrue;
    n->ifstmt.iffalse = iffalse;

    return n;
}

Node *mkloopstmt(int line, Node *init, Node *cond, Node *incr, Node *body)
{
    Node *n;

    n = mknode(line, Nloopstmt);
    n->loopstmt.init = init;
    n->loopstmt.cond = cond;
    n->loopstmt.step = incr;
    n->loopstmt.body = body;

    return n;
}

Node *mkiterstmt(int line, Node *elt, Node *seq, Node *body)
{
    Node *n;

    n = mknode(line, Niterstmt);
    n->iterstmt.elt = elt;
    n->iterstmt.seq = seq;
    n->iterstmt.body = body;

    return n;
}

Node *mkmatchstmt(int line, Node *val, Node **matches, size_t nmatches)
{
    Node *n;

    n = mknode(line, Nmatchstmt);
    n->matchstmt.val = val;
    n->matchstmt.matches = matches;
    n->matchstmt.nmatches = nmatches;
    return n;
}

Node *mkmatch(int line, Node *pat, Node *body)
{
    Node *n;

    n = mknode(line, Nmatch);
    n->match.pat = pat;
    n->match.block = body;
    return n;
}

Node *mkfunc(int line, Node **args, size_t nargs, Type *ret, Node *body)
{
    Node *n;
    Node *f;
    size_t i;

    f = mknode(line, Nfunc);
    f->func.args = args;
    f->func.nargs = nargs;
    f->func.body = body;
    f->func.scope = mkstab();
    f->func.type = mktyfunc(line, args, nargs, ret);

    for (i = 0; i < nargs; i++)
        putdcl(f->func.scope, args[i]);

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

Node *mktrait(int line, Node *name, Node **funcs, size_t nfuncs, Node **membs, size_t nmembs)
{
    Node *n;

    n = mknode(line, Ntrait);
    n->trait.name = name;
    n->trait.funcs = funcs;
    n->trait.nfuncs = nfuncs;
    n->trait.membs = membs;
    n->trait.nmembs = nmembs;
    return n;
}

Node *mkintlit(int line, uvlong val)
{
    return mkexpr(line, Olit, mkint(line, val), NULL);
}

Node *mklbl(int line, char *lbl)
{
    Node *n;

    assert(lbl != NULL);
    n = mknode(line, Nlit);
    n->lit.littype = Llbl;
    n->lit.lblval = strdup(lbl);
    return mkexpr(line, Olit, n, NULL);
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

Node *mkidxinit(int line, Node *idx, Node *init)
{
    init->expr.idx = idx;
    return init;
}

Node *mkname(int line, char *name)
{
    Node *n;

    n = mknode(line, Nname);
    n->name.name = strdup(name);

    return n;
}

Node *mknsname(int line, char *ns, char *name)
{
    Node *n;

    n = mknode(line, Nname);
    n->name.ns = strdup(ns);
    n->name.name = strdup(name);

    return n;
}

Node *mkdecl(int line, Node *name, Type *ty)
{
    Node *n;

    n = mknode(line, Ndecl);
    n->decl.did = ndecls;
    n->decl.name = name;
    n->decl.type = ty;
    lappend(&decls, &ndecls, n);
    return n;
}

Ucon *mkucon(int line, Node *name, Type *ut, Type *et)
{
    Ucon *uc;

    uc = zalloc(sizeof(Ucon));
    uc->line = line;
    uc->name = name;
    uc->utype = ut;
    uc->etype = et;
    return uc;
}

Node *mkbool(int line, int val)
{
    Node *n;

    n = mknode(line, Nlit);
    n->lit.littype = Lbool;
    n->lit.boolval = val;

    return n;
}

char *declname(Node *n)
{
    Node *name;
    assert(n->type == Ndecl);
    name = n->decl.name;
    return name->name.name;
}

Type *decltype(Node *n)
{
    assert(n->type == Ndecl);
    return nodetype(n);
}

Type *exprtype(Node *n)
{
    assert(n->type == Nexpr);
    return nodetype(n);
}

Type *nodetype(Node *n)
{
    switch (n->type) {
        case Ndecl:     return n->decl.type;            break;
        case Nexpr:     return n->expr.type;            break;
        case Nlit:      return n->lit.type;             break;
        default:        die("Node %s has no type", nodestr(n->type)); break;
    }
    return NULL;
}

/* name hashing */
ulong namehash(void *p)
{
    Node *n;

    n = p;
    return strhash(namestr(n)) ^ strhash(n->name.ns);
}

int nameeq(void *p1, void *p2)
{
    Node *a, *b;
    a = p1;
    b = p2;
    if (a == b)
        return 1;

    return streq(namestr(a), namestr(b)) && streq(a->name.ns, b->name.ns);
}

void setns(Node *n, char *ns)
{
    n->name.ns = strdup(ns);
}

Op exprop(Node *e)
{
    assert(e->type == Nexpr);
    return e->expr.op;
}

char *namestr(Node *name)
{
    if (!name)
        return "";
    assert(name->type == Nname);
    return name->name.name;
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

/* Hashes a Ovar expr or an Ndecl  */
ulong varhash(void *dcl)
{
    /* large-prime hash. meh. */
    return did(dcl) * 366787;
}

/* Checks if the did of two vars are equal */
int vareq(void *a, void *b)
{
    return did(a) == did(b);
}
