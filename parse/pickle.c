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
static void wrsym(FILE *fd, Sym *val);
static Sym *rdsym(FILE *fd);

static void be64(vlong v, char buf[8])
{
    buf[0] = (v >> 56) & 0xff;
    buf[1] = (v >> 48) & 0xff;
    buf[2] = (v >> 40) & 0xff;
    buf[3] = (v >> 32) & 0xff;
    buf[4] = (v >> 24) & 0xff;
    buf[5] = (v >> 16) & 0xff;
    buf[6] = (v >> 8)  & 0xff;
    buf[7] = (v >> 0)  & 0xff;
}

static vlong host64(char buf[8])
{
    vlong v = 0;

    v |= ((vlong)buf[0] << 56LL) & 0xff;
    v |= ((vlong)buf[1] << 48LL) & 0xff;
    v |= ((vlong)buf[2] << 40LL) & 0xff;
    v |= ((vlong)buf[3] << 32LL) & 0xff;
    v |= ((vlong)buf[4] << 24LL) & 0xff;
    v |= ((vlong)buf[5] << 16LL) & 0xff;
    v |= ((vlong)buf[6] << 8LL)  & 0xff;
    v |= ((vlong)buf[7] << 0LL)  & 0xff;
    return v;
}

static void be32(long v, char buf[4])
{
    buf[0] = (v >> 24) & 0xff;
    buf[1] = (v >> 16) & 0xff;
    buf[2] = (v >> 8)  & 0xff;
    buf[3] = (v >> 0)  & 0xff;
}

static long host32(char buf[4])
{
    long v = 0;
    v |= (buf[4] << 24) & 0xff;
    v |= (buf[5] << 16) & 0xff;
    v |= (buf[6] << 8)  & 0xff;
    v |= (buf[7] << 0)  & 0xff;
    return v;
}

static void wrbyte(FILE *fd, char val)
{
    if (fputc(val, fd) == EOF)
        die("Unexpected EOF");
}

static char rdbyte(FILE *fd)
{
    int c;
    c = fgetc(fd);
    if (c == EOF)
        die("Unexpected EOF");
    return c;
}

static void wrint(FILE *fd, long val)
{
    char buf[4];
    be32(val, buf);
    if (fwrite(buf, 4, 1, fd) < 4)
        die("Unexpected EOF");
}

static long rdint(FILE *fd)
{
    char buf[4];
    if (fread(buf, 4, 1, fd) < 4)
        die("Unexpected EOF");
    return host32(buf);
}

static void wrstr(FILE *fd, char *val)
{
    size_t len;
    size_t n;

    if (!val) {
        wrint(fd, -1);
    } else {
        wrint(fd, strlen(val));
        len = strlen(val);
        n = 0;
        while (n < len) {
            n += fwrite(val, len - n, 1, fd);
            if (feof(fd) || ferror(fd))
                die("Unexpected EOF");
        }
    }
}

/*static */char *rdstr(FILE *fd)
{
    ssize_t len;
    char *s;

    len = rdint(fd);
    if (len == -1) {
        return NULL;
    } else {
        s = xalloc(len + 1);
        if (fread(s, len, 1, fd) != (size_t)len)
            die("Unexpected EOF");
        s[len] = '\0';
        return s;
    }
}

static void wrflt(FILE *fd, double val)
{
    char buf[8];
    /* Assumption: We have 'val' in 64 bit IEEE format */
    union {
        uvlong ival;
        double fval;
    } u;

    u.fval = val;
    be64(u.ival, buf);
    if (fwrite(buf, 8, 1, fd) < 8)
        die("Unexpected EOF");
}

/*static */double rdflt(FILE *fd)
{
    char buf[8];
    union {
        uvlong ival;
        double fval;
    } u;

    if (fread(buf, 8, 1, fd) < 8)
        die("Unexpected EOF");
    u.ival = host64(buf);
    return u.fval;
}

static void wrbool(FILE *fd, int val)
{
    wrbyte(fd, val);
}

static int rdbool(FILE *fd)
{
    return rdbyte(fd);
}

static void wrstab(FILE *fd, Stab *val)
{
    int n, i;
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

static void wrsym(FILE *fd, Sym *val)
{
    wrint(fd, val->line);
    pickle(val->name, fd);
    wrtype(fd, val->type);
}

static Sym *rdsym(FILE *fd)
{
    int line;
    Node *name;
    Type *type;

    line = rdint(fd);
    name = unpickle(fd);
    type = rdtype(fd);
    return mksym(line, name, type);
}


static void wrtype(FILE *fd, Type *ty)
{
    size_t i;

    if (!ty) {
        die("trying to pickle null type\n");
        return;
    }
    printf("Writing %s\n", tystr(ty));
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
        case Tyenum:
            wrint(fd, ty->nmemb);
            for (i = 0; i < ty->nmemb; i++)
                pickle(ty->edecls[i], fd);
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
        case Tyenum:
            ty->nmemb = rdint(fd);
            ty->edecls = xalloc(ty->nmemb * sizeof(Node*));
            for (i = 0; i < ty->nmemb; i++)
                ty->edecls[i] = unpickle(fd);
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
    printf("Read %s\n", tystr(ty));
    return ty;
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
            wrint(fd, n->name.nparts);
            for (i = 0; i < n->name.nparts; i++)
                wrstr(fd, n->name.parts[i]);
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
            wrsym(fd, n->decl.sym);
            wrint(fd, n->decl.flags);
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
            n->name.nparts = rdint(fd);
            n->name.parts = xalloc(sizeof(Node *)*n->name.nparts);
            for (i = 0; i < n->name.nparts; i++)
                n->name.parts[i] = rdstr(fd);
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
            n->decl.sym = rdsym(fd);
            n->decl.flags = rdint(fd);
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
