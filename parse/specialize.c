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

static Node *specializenode(Node *n, Htab *tsmap);

/*
 * Checks if a type contains any type
 * parameers at all (ie, if it generic).
 */
static int hasparams(Type *t)
{
    size_t i;

    if (t->type == Typaram || t->isgeneric)
        return 1;
    for (i = 0; i < t->nsub; i++)
        if (hasparams(t->sub[i]))
            return 1;
    for (i = 0; i < t->narg; i++)
        if (hasparams(t->arg[i]))
            return 1;
    return 0;
}

/*
 * Duplicates the type 't', with all bound type
 * parameters substituted with the substitions
 * described in 'tsmap'
 *
 * Returns a fresh type with all unbound type
 * parameters (type schemes in most literature)
 * replaced with type variables that we can unify
 * against */
Type *tyspecialize(Type *t, Htab *tsmap)
{
    Type *ret, *tmp;
    size_t i;
    Type **subst;

    if (hthas(tsmap, t))
        return htget(tsmap, t);
    switch (t->type) {
        case Typaram:
            ret = mktyvar(t->line);
            htput(tsmap, t, ret);
            break;
        case Tyname:
            if (!hasparams(t)) {
                ret = t;
            } else {
                if (t->narg)
                    subst = t->arg;
                else
                    subst = t->param;
                for (i = 0; i < t->nparam; i++) {
                    if (subst[i]->type != Typaram || hthas(tsmap, subst[i]))
                        continue;
                    tmp = mktyvar(subst[i]->line);
                    htput(tsmap, subst[i], tmp);
                }
                ret = mktyname(t->line, t->name, t->param, t->nparam, tyspecialize(t->sub[0], tsmap));
                ret->issynth = 1;
                htput(tsmap, t, ret);
                for (i = 0; i < t->nparam; i++)
                    lappend(&ret->arg, &ret->narg, tyspecialize(subst[i], tsmap));
                ret->isgeneric = hasparams(ret);
            }
            break;
        case Tystruct:
            ret = tydup(t);
            htput(tsmap, t, ret);
            pushstab(NULL);
            for (i = 0; i < t->nmemb; i++)
                ret->sdecls[i] = specializenode(t->sdecls[i], tsmap);
            popstab();
            break;
        case Tyunion:
            ret = tydup(t);
            htput(tsmap, t, ret);
            for (i = 0; i < t->nmemb; i++) {
                tmp = NULL;
                if (ret->udecls[i]->etype)
                    tmp = tyspecialize(t->udecls[i]->etype, tsmap);
                ret->udecls[i] = mkucon(t->line, t->udecls[i]->name, ret, tmp);
                ret->udecls[i]->utype = ret;
                ret->udecls[i]->id = i;
                ret->udecls[i]->synth = 1;
            }
            break;
        default:
            if (t->nsub > 0) {
                ret = tydup(t);
                htput(tsmap, t, ret);
                for (i = 0; i < t->nsub; i++)
                    ret->sub[i] = tyspecialize(t->sub[i], tsmap);
            } else {
                ret = t;
            }
            break;
    }
    return ret;
}

/* Checks if the type 't' is generic, and if it is
 * substitutes the types. This is here for efficiency,
 * so we don't gratuitously duplicate types */
static Type *tysubst(Type *t, Htab *tsmap)
{
    if (hasparams(t))
        return tyspecialize(t, tsmap);
    else
        return t;
}

/* 
 * Fills the substitution map with a mapping from
 * the type parameter 'from' to it's substititon 'to'
 */
static void fillsubst(Htab *tsmap, Type *to, Type *from)
{
    size_t i;

    if (from->type == Typaram) {
        if (debugopt['S'])
            printf("mapping %s => %s\n", tystr(from), tystr(to));
        htput(tsmap, from, to);
        return;
    }
    assert(to->nsub == from->nsub);
    for (i = 0; i < to->nsub; i++)
        fillsubst(tsmap, to->sub[i], from->sub[i]);
    if (to->type == Tyname && to->nparam > 0) {
        assert(to->nparam == to->narg);
        for (i = 0; i < to->nparam; i++)
            fillsubst(tsmap, to->arg[i], to->param[i]);
    }
}

/*
 * Fixes up nodes. This involves fixing up the
 * declaration identifiers once we specialize
 */
static void fixup(Node *n)
{
    size_t i;
    Node *d;
    Stab *ns;

    if (!n)
        return;
    switch (n->type) {
        case Nfile:
        case Nuse:
            die("Node %s not allowed here\n", nodestr(n->type));
            break;
        case Nexpr:
            fixup(n->expr.idx);
            for (i = 0; i < n->expr.nargs; i++)
                fixup(n->expr.args[i]);
            if (n->expr.op == Ovar) {
                ns = curstab();
                if (n->expr.args[0]->name.ns)
                    ns = getns_str(ns, n->expr.args[0]->name.ns);
                if (!ns)
                    fatal(n->line, "No namespace %s\n", n->expr.args[0]->name.ns);
                d = getdcl(ns, n->expr.args[0]);
                if (!d)
                    die("Missing decl %s", namestr(n->expr.args[0]));
                if (d->decl.isgeneric)
                    d = specializedcl(d, n->expr.type, &n->expr.args[0]);
                n->expr.did = d->decl.did;
            }
            break;
        case Nlit:
            switch (n->lit.littype) {
                case Lfunc:     fixup(n->lit.fnval);          break;
                case Lchr: case Lint: case Lflt:
                case Lstr: case Llbl: case Lbool:
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
            break;
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
        case Nnone: case Nname:
            break;
    }
}


/*
 * Duplicates a node, replacing all things that
 * need to be specialized to make it concrete
 * instead of generic, and returns it.
 */
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
            r->expr.idx = specializenode(n->expr.idx, tsmap);
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
                case Llbl:      r->lit.lblval = n->lit.lblval;       break;
                case Lbool:     r->lit.boolval = n->lit.boolval;     break;
                case Lfunc:     r->lit.fnval = specializenode(n->lit.fnval, tsmap);       break;
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
        case Ndecl:
            r->decl.did = ndecls;
            /* sym */
            r->decl.name = specializenode(n->decl.name, tsmap);
            r->decl.type = tysubst(n->decl.type, tsmap);

            /* symflags */
            r->decl.isconst = n->decl.isconst;
            r->decl.isgeneric = n->decl.isgeneric;
            r->decl.isextern = n->decl.isextern;
            if (curstab())
                putdcl(curstab(), r);

            /* init */
            r->decl.init = specializenode(n->decl.init, tsmap);
            lappend(&decls, &ndecls, r);
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
    p += snprintf(p, end - p, "$%d", (int)t->type);
    for (i = 0; i < t->nsub; i++)
        p += tidappend(p, end - p, t->sub[i]);
    return p - buf;
}

static Node *genericname(Node *n, Type *t)
{
    char buf[1024];
    char *p;
    char *end;
    Node *name;

    if (!n->decl.isgeneric)
        return n->decl.name;
    p = buf;
    end = buf + 1024;
    p += snprintf(p, end - p, "%s", n->decl.name->name.name);
    p += snprintf(p, end - p, "$%zd", n->decl.did);
    tidappend(p, end - p, t);
    name = mkname(n->line, buf);
    if (n->decl.name->name.ns)
        setns(name, n->decl.name->name.ns);
    return name;
}

/*
 * Takes a generic declaration, and creates a specialized
 * duplicate of it with type 'to'. It also generates
 * a name for this specialized node, and returns it in '*name'.
 */
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
    if (debugopt['S'])
        printf("specializing %s => %s\n", namestr(n->decl.name), namestr(*name));
    if (d)
        return d;
    /* namespaced names need to be looked up in their correct
     * context. */
    if (n->decl.name->name.ns) {
        ns = mkname(n->line, n->decl.name->name.ns);
        st = getns(file->file.globls, ns);
        pushstab(st);
    }

    /* specialize */
    tsmap = mkht(tyhash, tyeq);
    fillsubst(tsmap, to, n->decl.type);

    d = mkdecl(n->line, *name, tysubst(n->decl.type, tsmap));
    d->decl.isconst = n->decl.isconst;
    d->decl.isextern = n->decl.isextern;
    d->decl.init = specializenode(n->decl.init, tsmap);
    putdcl(file->file.globls, d);

    fixup(d);

    lappend(&file->file.stmts, &file->file.nstmts, d);
    if (d->decl.name->name.ns)
        popstab();
    return d;
}
