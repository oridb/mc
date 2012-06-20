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
#include <arpa/inet.h>

#include "parse.h"

static void wrtype(FILE *fd, Type *val);
static Type *rdtype(FILE *fd);
static void wrstab(FILE *fd, Stab *val);
static Stab *rdstab(FILE *fd);
static void wrsym(FILE *fd, Node *val);
static Node *rdsym(FILE *fd);

static void wrstab(FILE *fd, Stab *val)
{
    size_t n, i;
    void **keys;

    pickle(val->name, fd);

    /* write decls */
    keys = htkeys(val->dcl, &n);
    wrint(fd, n);
    for (i = 0; i < n; i++)
        wrsym(fd, getdcl(val, keys[i]));
    free(keys);

    /* write types */
    keys = htkeys(val->ty, &n);
    wrint(fd, n);
    for (i = 0; i < n; i++) {
        pickle(keys[i], fd); /* name */
        wrtype(fd, gettype(val, keys[i])); /* type */
    }
    free(keys);

    /* write stabs */
    keys = htkeys(val->ns, &n);
    wrint(fd, n);
    for (i = 0; i < n; i++)
        wrstab(fd, getns(val, keys[i]));
    free(keys);
}

static Stab *rdstab(FILE *fd)
{
    Stab *st;
    Type *ty;
    Node *nm;
    int n;
    int i;

    /* read dcls */
    st = mkstab();
    st->name = unpickle(fd);
    n = rdint(fd);
    for (i = 0; i < n; i++)
         putdcl(st, rdsym(fd));

    /* read types */
    n = rdint(fd);
    for (i = 0; i < n; i++) {
        nm = unpickle(fd);
        ty = rdtype(fd);
        puttype(st, nm, ty);
    }

    /* read stabs */
    n = rdint(fd);
    for (i = 0; i < n; i++)
        putns(st, rdstab(fd));

    return st;
}

static void wrsym(FILE *fd, Node *val)
{
    /* sym */
    wrint(fd, val->line);
    pickle(val->decl.name, fd);
    wrtype(fd, val->decl.type);

    /* symflags */
    wrint(fd, val->decl.isconst);
    wrint(fd, val->decl.isgeneric);
    wrint(fd, val->decl.isextern);
}

static Node *rdsym(FILE *fd)
{
    int line;
    Node *name;
    Type *type;
    Node *n;

    line = rdint(fd);
    name = unpickle(fd);
    type = rdtype(fd);
    n = mkdecl(line, name, type);
    
    n->decl.isconst = rdint(fd);
    n->decl.isgeneric = rdint(fd);
    n->decl.isextern = rdint(fd);
    return n;
}

Type *tyunpickle(FILE *fd)
{
    return rdtype(fd);
}

Node *symunpickle(FILE *fd)
{
    return rdsym(fd);
}

static void wrtype(FILE *fd, Type *ty)
{
    size_t i;

    if (!ty) {
        die("trying to pickle null type\n");
        return;
    }
    wrbyte(fd, ty->type);
    /* tid is generated; don't write */
    /* cstrs are left out for now: FIXME */
    wrint(fd, ty->nsub);
    switch (ty->type) {
        case Tyname:
            pickle(ty->name, fd);
            break;
        case Typaram:
            wrstr(fd, ty->pname);
            break;
        case Tystruct:
            wrint(fd, ty->nmemb);
            for (i = 0; i < ty->nmemb; i++)
                pickle(ty->sdecls[i], fd);
            break;
        case Tyunion:
            wrint(fd, ty->nmemb);
            for (i = 0; i < ty->nmemb; i++)
                pickle(ty->udecls[i], fd);
            break;
        case Tyarray:
            wrtype(fd, ty->sub[0]);
            pickle(ty->asize, fd);
            break;
        case Tyslice:
            wrtype(fd, ty->sub[0]);
            break;
        case Tyvar:
            die("Attempting to pickle %s. This will not work.\n", tystr(ty));
            break;
        default:
            for (i = 0; i < ty->nsub; i++)
                wrtype(fd, ty->sub[i]);
            break;
    }
}

static Type *rdtype(FILE *fd)
{
    Type *ty;
    Ty t;
    size_t i;

    t = rdbyte(fd);
    ty = mkty(-1, t);
    /* tid is generated; don't write */
    /* cstrs are left out for now: FIXME */
    ty->nsub = rdint(fd);
    if (ty->nsub > 0)
        ty->sub = xalloc(ty->nsub * sizeof(Type*));
    switch (ty->type) {
        case Tyname:
            ty->name = unpickle(fd);
            break;
        case Typaram:
            ty->pname = rdstr(fd);
            break;
        case Tystruct:
            ty->nmemb = rdint(fd);
            ty->sdecls = xalloc(ty->nmemb * sizeof(Node*));
            for (i = 0; i < ty->nmemb; i++)
                ty->sdecls[i] = unpickle(fd);
            break;
        case Tyunion:
            ty->nmemb = rdint(fd);
            ty->udecls = xalloc(ty->nmemb * sizeof(Node*));
            for (i = 0; i < ty->nmemb; i++)
                ty->udecls[i] = unpickle(fd);
            break;
        case Tyarray:
            ty->sub[0] = rdtype(fd);
            ty->asize = unpickle(fd);
            break;
        case Tyslice:
            ty->sub[0] = rdtype(fd);
            break;
        default:
            for (i = 0; i < ty->nsub; i++)
                ty->sub[i] = rdtype(fd);
            break;
    }
    return ty;
}

void typickle(Type *t, FILE *fd)
{
    wrtype(fd, t);
}

void sympickle(Node *s, FILE *fd)
{
    wrsym(fd, s);
}

/* pickle format:
 *      type:byte
 *      node-attrs: string|size|
 *      nsub:int32_be
 *      sub:node[,]
 */
void pickle(Node *n, FILE *fd)
{
    size_t i;

    if (!n) {
        wrbyte(fd, Nnone);
        return;
    }
    wrbyte(fd, n->type);
    wrint(fd, n->line);
    switch (n->type) {
        case Nfile:
            wrstr(fd, n->file.name);
            wrint(fd, n->file.nuses);
            for (i = 0; i < n->file.nuses; i++)
                pickle(n->file.uses[i], fd);
            wrint(fd, n->file.nstmts);
            for (i = 0; i < n->file.nstmts; i++)
                pickle(n->file.stmts[i], fd);
            wrstab(fd, n->file.globls);
            wrstab(fd, n->file.exports);
            break;

        case Nexpr:
            wrbyte(fd, n->expr.op);
            wrtype(fd, n->expr.type);
            wrbool(fd, n->expr.isconst);
            wrint(fd, n->expr.nargs);
            for (i = 0; i < n->expr.nargs; i++)
                pickle(n->expr.args[i], fd);
            break;
        case Nname:
            wrbool(fd, n->name.ns != NULL);
            if (n->name.ns) {
                wrstr(fd, n->name.ns);
            }
            wrstr(fd, n->name.name);
            break;
        case Nuse:
            wrbool(fd, n->use.islocal);
            wrstr(fd, n->use.name);
            break;
        case Nlit:
            wrbyte(fd, n->lit.littype);
            wrtype(fd, n->lit.type);
            switch (n->lit.littype) {
                case Lchr:      wrint(fd, n->lit.chrval);       break;
                case Lint:      wrint(fd, n->lit.intval);       break;
                case Lflt:      wrflt(fd, n->lit.fltval);       break;
                case Lstr:      wrstr(fd, n->lit.strval);       break;
                case Lbool:     wrbool(fd, n->lit.boolval);     break;
                case Lfunc:     pickle(n->lit.fnval, fd);       break;
                case Larray:    pickle(n->lit.arrval, fd);      break;
            }
            break;
        case Nloopstmt:
            pickle(n->loopstmt.init, fd);
            pickle(n->loopstmt.cond, fd);
            pickle(n->loopstmt.step, fd);
            pickle(n->loopstmt.body, fd);
            break;
        case Nifstmt:
            pickle(n->ifstmt.cond, fd);
            pickle(n->ifstmt.iftrue, fd);
            pickle(n->ifstmt.iffalse, fd);
            break;
        case Nblock:
            wrstab(fd, n->block.scope);
            wrint(fd, n->block.nstmts);
            for (i = 0; i < n->block.nstmts; i++)
                pickle(n->block.stmts[i], fd);
            break;
        case Nlbl:
            wrstr(fd, n->lbl.name);
            break;
        case Ndecl:
            /* sym */
            pickle(n->decl.name, fd);
            wrtype(fd, n->decl.type);

            /* symflags */
            wrint(fd, n->decl.isconst);
            wrint(fd, n->decl.isgeneric);
            wrint(fd, n->decl.isextern);

            /* init */
            pickle(n->decl.init, fd);
            break;
        case Nfunc:
            wrstab(fd, n->func.scope);
            wrint(fd, n->func.nargs);
            for (i = 0; i < n->func.nargs; i++)
                pickle(n->func.args[i], fd);
            pickle(n->func.body, fd);
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
}

Node *unpickle(FILE *fd)
{
    size_t i;
    Ntype type;
    Node *n;

    type = rdbyte(fd);
    if (type == Nnone)
        return NULL;
    n = mknode(-1, type);
    n->line = rdint(fd);
    switch (n->type) {
        case Nfile:
            n->file.name = rdstr(fd);
            n->file.nuses = rdint(fd);
            n->file.uses = xalloc(sizeof(Node*)*n->file.nuses);
            for (i = 0; i < n->file.nuses; i++)
                n->file.uses[i] = unpickle(fd);
            n->file.nstmts = rdint(fd);
            n->file.stmts = xalloc(sizeof(Node*)*n->file.nstmts);
            for (i = 0; i < n->file.nstmts; i++)
                n->file.stmts[i] = unpickle(fd);
            n->file.globls = rdstab(fd);
            n->file.exports = rdstab(fd);
            break;

        case Nexpr:
            n->expr.op = rdbyte(fd);
            n->expr.type = rdtype(fd);
            n->expr.isconst = rdbool(fd);
            n->expr.nargs = rdint(fd);
            n->expr.args = xalloc(sizeof(Node *)*n->expr.nargs);
            for (i = 0; i < n->expr.nargs; i++)
                n->expr.args[i] = unpickle(fd);
            break;
        case Nname:
            if (rdbool(fd))
                n->name.ns = rdstr(fd);
            n->name.name = rdstr(fd);
            break;
        case Nuse:
            n->use.islocal = rdbool(fd);
            n->use.name = rdstr(fd);
            break;
        case Nlit:
            n->lit.littype = rdbyte(fd);
            n->lit.type = rdtype(fd);
            switch (n->lit.littype) {
                case Lchr:      n->lit.chrval = rdint(fd);       break;
                case Lint:      n->lit.intval = rdint(fd);       break;
                case Lflt:      n->lit.fltval = rdflt(fd);       break;
                case Lstr:      n->lit.strval = rdstr(fd);       break;
                case Lbool:     n->lit.boolval = rdbool(fd);     break;
                case Lfunc:     n->lit.fnval = unpickle(fd);       break;
                case Larray:    n->lit.arrval = unpickle(fd);      break;
            }
            break;
        case Nloopstmt:
            n->loopstmt.init = unpickle(fd);
            n->loopstmt.cond = unpickle(fd);
            n->loopstmt.step = unpickle(fd);
            n->loopstmt.body = unpickle(fd);
            break;
        case Nifstmt:
            n->ifstmt.cond = unpickle(fd);
            n->ifstmt.iftrue = unpickle(fd);
            n->ifstmt.iffalse = unpickle(fd);
            break;
        case Nblock:
            n->block.scope = rdstab(fd);
            n->block.nstmts = rdint(fd);
            n->block.stmts = xalloc(sizeof(Node *)*n->block.nstmts);
            for (i = 0; i < n->block.nstmts; i++)
                n->block.stmts[i] = unpickle(fd);
            break;
        case Nlbl:
            n->lbl.name = rdstr(fd);
            break;
        case Ndecl:
            /* sym */
            n->decl.name = unpickle(fd);
            n->decl.type = rdtype(fd);

            /* symflags */
            n->decl.isconst = rdint(fd);
            n->decl.isgeneric = rdint(fd);
            n->decl.isextern = rdint(fd);

            /* init */
            n->decl.init = unpickle(fd);
            break;
        case Nfunc:
            n->func.scope = rdstab(fd);
            n->func.nargs = rdint(fd);
            n->func.args = xalloc(sizeof(Node *)*n->func.nargs);
            for (i = 0; i < n->func.nargs; i++)
                n->func.args[i] = unpickle(fd);
            n->func.body = unpickle(fd);
            break;
        case Nnone:
            die("Nnone should not be seen as node type!");
            break;
    }
    return n;
}
