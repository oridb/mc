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

static ulong tyhash(void *p)
{
    Type *t;

    t = p;
    return strhash(t->pname);
}

static int tyeq(void *pa, void *pb)
{
    Type *a, *b;

    a = pa;
    b = pb;
    return streq(a->pname, b->pname);
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

    if (t->type == Typaram) {
        ret = htget(tsmap, t);
    } else {
        ret = tydup(t);
        for (i = 0; i < t->nsub; i++)
            ret->sub[i] = dosubst(t->sub[i], tsmap);
    }
    assert(ret != NULL);
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

    if (from->type == Typaram) {
        htput(tsmap, from, to);
    }
    if (to->nsub != from->nsub)
        return;
    for (i = 0; i < to->nsub; i++)
        fillsubst(tsmap, to->sub[i], from->sub[i]);
}

static void fixup(Node *n)
{
    size_t i;
    Node *d;

    if (!n)
        return;
    switch (n->type) {
        case Nfile:
        case Nuse:
            die("Node %s not allowed here\n", nodestr(n->type));
            break;
        case Nexpr:
            for (i = 0; i < n->expr.nargs; i++)
                fixup(n->expr.args[i]);
            if (n->expr.op == Ovar) {
                d = getdcl(curstab(), n->expr.args[0]);
                if (!d)
                    die("Missing decl %s\n", namestr(n->expr.args[0]));
                n->expr.did = d->decl.did;
            }
            break;
        case Nlit:
            switch (n->lit.littype) {
                case Lfunc:     fixup(n->lit.fnval);          break;
                case Lseq:
                    for (i = 0; i < n->lit.nelt; i++)
                        fixup(n->lit.seqval[i]);
                    break;
                case Lchr: case Lint: case Lflt: case Lstr: case Lbool:
                    break;
            }
            break;
        case Nifstmt:
            fixup(n->ifstmt.cond);
            fixup(n->ifstmt.iftrue);
            fixup(n->ifstmt.iffalse);
            break;
        case Nloopstmt:
            fixup(n->loopstmt.init);
            fixup(n->loopstmt.cond);
            fixup(n->loopstmt.step);
            fixup(n->loopstmt.body);
            break;
        case Nmatchstmt:
            fixup(n->matchstmt.val);
            for (i = 0; i < n->matchstmt.nmatches; i++)
                fixup(n->matchstmt.matches[i]);
        case Nmatch:
            fixup(n->match.pat);
            fixup(n->match.block);
            break;
        case Nblock:
            pushstab(n->block.scope);
            for (i = 0; i < n->block.nstmts; i++)
                fixup(n->block.stmts[i]);
            popstab();
            break;
        case Ndecl:
            fixup(n->decl.init);
            break;
        case Nfunc:
            pushstab(n->func.scope);
            fixup(n->func.body);
            popstab();
            break;
        case Nnone: case Nlbl: case Nname:
            break;
    }
}

static Node *specializenode(Node *n, Htab *tsmap)
{
    Node *r;
    size_t i;

    if (!n)
        return NULL;
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
                case Lchr:      r->lit.chrval = n->lit.chrval;       break;
                case Lint:      r->lit.intval = n->lit.intval;       break;
                case Lflt:      r->lit.fltval = n->lit.fltval;       break;
                case Lstr:      r->lit.strval = n->lit.strval;       break;
                case Lbool:     r->lit.boolval = n->lit.boolval;     break;
                case Lfunc:     r->lit.fnval = specializenode(n->lit.fnval, tsmap);       break;
                case Lseq:
                    for (i = 0; i < n->lit.nelt; i++)
                        r->lit.seqval[i] = specializenode(n->lit.seqval[i], tsmap);
                    break;
            }
            break;
        case Nifstmt:
            r->ifstmt.cond = specializenode(n->ifstmt.cond, tsmap);
            r->ifstmt.iftrue = specializenode(n->ifstmt.iftrue, tsmap);
            r->ifstmt.iffalse = specializenode(n->ifstmt.iffalse, tsmap);
            break;
        case Nloopstmt:
            r->loopstmt.init = specializenode(n->loopstmt.init, tsmap);
            r->loopstmt.cond = specializenode(n->loopstmt.cond, tsmap);
            r->loopstmt.step = specializenode(n->loopstmt.step, tsmap);
            r->loopstmt.body = specializenode(n->loopstmt.body, tsmap);
            break;
        case Nmatchstmt:
            r->matchstmt.val = specializenode(n->matchstmt.val, tsmap);
            r->matchstmt.nmatches = n->matchstmt.nmatches;
            r->matchstmt.matches = xalloc(n->matchstmt.nmatches * sizeof(Node*));
            for (i = 0; i < n->matchstmt.nmatches; i++)
                r->matchstmt.matches[i] = specializenode(n->matchstmt.matches[i], tsmap);
            break;
        case Nmatch:
            r->match.pat = specializenode(n->match.pat, tsmap);
            r->match.block = specializenode(n->match.block, tsmap);
            break;
        case Nblock:
            r->block.scope = mkstab();
            r->block.scope->super = curstab();
            pushstab(r->block.scope);
            r->block.nstmts = n->block.nstmts;
            r->block.stmts = xalloc(sizeof(Node *)*n->block.nstmts);
            for (i = 0; i < n->block.nstmts; i++)
                r->block.stmts[i] = specializenode(n->block.stmts[i], tsmap);
            popstab();
            break;
        case Nlbl:
            r->lbl.name = strdup(n->lbl.name);
            break;
        case Ndecl:
            r->decl.did = maxdid++;
            /* sym */
            r->decl.name = specializenode(n->decl.name, tsmap);
            r->decl.type = tysubst(n->decl.type, tsmap);

            /* symflags */
            r->decl.isconst = n->decl.isconst;
            r->decl.isgeneric = n->decl.isgeneric;
            r->decl.isextern = n->decl.isextern;
            putdcl(curstab(), r);

            /* init */
            r->decl.init = specializenode(n->decl.init, tsmap);
            break;
        case Nfunc:
            r->func.scope = mkstab();
            r->func.scope->super = curstab();
            pushstab(r->func.scope);
            r->func.type = tysubst(n->func.type, tsmap);
            r->func.nargs = n->func.nargs;
            r->func.args = xalloc(sizeof(Node *)*n->func.nargs);
            for (i = 0; i < n->func.nargs; i++)
                r->func.args[i] = specializenode(n->func.args[i], tsmap);
            r->func.body = specializenode(n->func.body, tsmap);
            popstab();
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
    return r;
}

static size_t tidappend(char *buf, size_t sz, Type *t)
{
    char *p;
    char *end;
    size_t i;

    p = buf;
    end = buf + sz;
    p += snprintf(p, end - p, "$%d", t->tid);
    for (i = 0; i < t->nsub; i++)
        p += tidappend(p, end - p, t->sub[i]);
    return end - p;
}

static Node *genericname(Node *n, Type *t)
{
    char buf[1024];
    char *p;
    char *end;

    p = buf;
    end = buf + 1024;
    p += snprintf(p, end - p, "%s", n->decl.name->name.name);
    tidappend(p, end - p, t);
    return mkname(n->line, buf);
}

Node *specializedcl(Node *n, Type *to, Node **name)
{
    Htab *tsmap;
    Node *d;
    Node *ns;
    Stab *st;

    assert(n->type == Ndecl);
    assert(n->decl.isgeneric);

    *name = genericname(n, to);
    d = getdcl(file->file.globls, *name);
    if (d)
        return d;
    /* namespaced names need to be looked up in their correct
     * context. */
    if (n->decl.name->name.ns) {
        ns = mkname(n->line, n->decl.name->name.ns);
        st = getns(file->file.globls, ns);
        pushstab(st);
    }



    tsmap = mkht(tyhash, tyeq);
    fillsubst(tsmap, to, n->decl.type);

    d = mkdecl(n->line, *name, tysubst(n->decl.type, tsmap));
    d->decl.isconst = n->decl.isconst;
    d->decl.isextern = n->decl.isextern;
    d->decl.init = specializenode(n->decl.init, tsmap);
    fixup(d);

    putdcl(file->file.globls, d);
    lappend(&file->file.stmts, &file->file.nstmts, d);
    if (d->decl.name->name.ns)
        popstab();
    return d;
}
