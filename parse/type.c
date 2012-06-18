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

typedef struct Typename Typename;
struct Typename {
    Ty ty;
    char *name;
};

Type **tytab = NULL;
int ntypes;
Cstr **cstrtab;
int ncstrs;

static Cstr *tycstrs[Ntypes + 1][4];

Type *mkty(int line, Ty ty)
{
    Type *t;
    int i;

    t = zalloc(sizeof(Type));
    t->type = ty;
    t->tid = ntypes++;
    t->line = line;
    tytab = xrealloc(tytab, ntypes*sizeof(Type*));
    tytab[t->tid] = NULL;

    for(i = 0; tycstrs[ty][i]; i++)
        constrain(t, tycstrs[ty][i]);

    return t;
}

Type *tylike(Type *t, Ty like)
{
    int i;

    for (i = 0; tycstrs[like][i]; i++)
        constrain(t, tycstrs[like][i]);
    return t;
}

/* steals memb, funcs */
Cstr *mkcstr(int line, char *name, Node **memb, size_t nmemb, Node **funcs, size_t nfuncs)
{
    Cstr *c;

    c = zalloc(sizeof(Cstr));
    c->name = strdup(name);
    c->memb = memb;
    c->nmemb = nmemb;
    c->funcs = funcs;
    c->nfuncs = nfuncs;
    c->cid = ncstrs++;

    cstrtab = xrealloc(cstrtab, ncstrs*sizeof(Cstr*));
    cstrtab[c->cid] = c;
    return c;
}

Type *mktyvar(int line)
{
    Type *t;

    t = mkty(line, Tyvar);
    return t;
}

Type *mktyparam(int line, char *name)
{
    Type *t;

    t = mkty(line, Typaram);
    t->pname = strdup(name);
    return t;
}

Type *mktynamed(int line, Node *name)
{
    Type *t;

    /* resolve it in the type inference stage */
    t = mkty(line, Tyname);
    t->name = name;
    return t;
}

Type *mktyarray(int line, Type *base, Node *sz)
{
    Type *t;

    t = mkty(line, Tyarray);
    t->nsub = 1;
    t->nmemb = 1; /* the size is a "member" */
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    t->asize = sz;

    return t;
}

Type *mktyslice(int line, Type *base)
{
    Type *t;

    t = mkty(line, Tyslice);
    t->nsub = 1;
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktyidxhack(int line, Type *base)
{
    Type *t;

    t = mkty(line, Tyvar);
    t->nsub = 1;
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktyptr(int line, Type *base)
{
    Type *t;

    t = mkty(line, Typtr);
    t->nsub = 1;
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret)
{
    Type *t;
    size_t i;

    t = mkty(line, Tyfunc);
    t->nsub = nargs + 1;
    t->sub = xalloc((1 + nargs)*sizeof(Type));
    t->sub[0] = ret;
    for (i = 0; i < nargs; i++)
        t->sub[i + 1] = nodetype(args[i]);
    return t;
}

Type *mktystruct(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tystruct);
    t->nmemb = ndecls;
    t->sdecls = memdup(decls, ndecls*sizeof(Node *));
    return t;
}

Type *mktyunion(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tyunion);
    t->nmemb = ndecls;
    t->udecls = decls;
    return t;
}

Type *mktyenum(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tyenum);
    t->nmemb = ndecls;
    t->edecls = decls;
    return t;
}

int istysigned(Type *t)
{
    switch (t->type) {
        case Tyint8: case Tyint16: case Tyint:
        case Tyint32: case Tyint64: case Tylong:
            return 1;
        default:
            return 0;
    }
}

static int namefmt(char *buf, size_t len, Node *n)
{
    char *p;
    char *end;

    p = buf;
    end = p + len;
    if (n->name.ns)
        p += snprintf(p, end - p, "%s.", n->name.ns);
    p += snprintf(p, end - p, "%s", n->name.name);
    return len - (end - p);
}

int constrain(Type *t, Cstr *c)
{
    if (!t->cstrs)
        t->cstrs = mkbs();
    bsput(t->cstrs, c->cid);
    return 1;
}

int hascstr(Type *t, Cstr *c)
{
    return t->cstrs && bshas(t->cstrs, c->cid);
}

static int cstrfmt(char *buf, size_t len, Type *t)
{
    char *p;
    char *end;
    char *sep;
    int i;

    if (!t->cstrs || !bscount(t->cstrs))
        return 0;

    p = buf;
    end = p + len;

    p += snprintf(p, end - p, " :: ");
    sep = "";
    for (i = 0; i < ncstrs; i++) {
        if (bshas(t->cstrs, i)) {
            p += snprintf(p, end - p, "%s%s", sep, cstrtab[i]->name);
            sep = ",";
        }
    }
    return p - buf;
}

static int fmtstruct(char *buf, size_t len, Type *t)
{
    size_t i;
    char *end, *p;
    char *name, *ty;

    p = buf;
    end = p + len;
    p += snprintf(p, end - p, "struct ");
    for (i = 0; i < t->nmemb; i++) {
        name = declname(t->edecls[i]);
        ty = tystr(decltype(t->edecls[i]));
        p += snprintf(p, end - p, "%s:%s; ", name, ty);
        free(ty);
    }
    p += snprintf(p, end - p, ";;");
    return p - buf;
}

static int fmtunion(char *buf, size_t len, Type *t)
{
    size_t i;
    *buf = 0;
    for (i = 0; i < t->nmemb; i++)
        dump(t->sdecls[i], stdout);
    return 0;
}

static int fmtenum(char *buf, size_t len, Type *t)
{
    size_t i;
    *buf = 0;
    for (i = 0; i < t->nmemb; i++)
        dump(t->sdecls[i], stdout);
    return 0;
}

static int tybfmt(char *buf, size_t len, Type *t)
{
    size_t i;
    char *p;
    char *end;
    char *sep;

    p = buf;
    end = p + len;
    sep = "";
    if (!t) {
        p += snprintf(p, end - p, "tynil");
        return len - (end - p);
    }
    switch (t->type) {
        case Tybad:     p += snprintf(p, end - p, "BAD");       break;
        case Tyvoid:    p += snprintf(p, end - p, "void");      break;
        case Tybool:    p += snprintf(p, end - p, "bool");      break;
        case Tychar:    p += snprintf(p, end - p, "char");      break;
        case Tyint8:    p += snprintf(p, end - p, "int8");      break;
        case Tyint16:   p += snprintf(p, end - p, "int16");     break;
        case Tyint:     p += snprintf(p, end - p, "int");       break;
        case Tyint32:   p += snprintf(p, end - p, "int32");     break;
        case Tyint64:   p += snprintf(p, end - p, "int64");     break;
        case Tylong:    p += snprintf(p, end - p, "long");      break;
        case Tybyte:    p += snprintf(p, end - p, "byte");      break;
        case Tyuint8:   p += snprintf(p, end - p, "uint8");     break;
        case Tyuint16:  p += snprintf(p, end - p, "uint16");    break;
        case Tyuint:    p += snprintf(p, end - p, "uint");      break;
        case Tyuint32:  p += snprintf(p, end - p, "uint32");    break;
        case Tyuint64:  p += snprintf(p, end - p, "uint64");    break;
        case Tyulong:   p += snprintf(p, end - p, "ulong");     break;
        case Tyfloat32: p += snprintf(p, end - p, "float32");   break;
        case Tyfloat64: p += snprintf(p, end - p, "float64");   break;
        case Tyvalist:  p += snprintf(p, end - p, "...");       break;

        case Typtr:     
            p += tybfmt(p, end - p, t->sub[0]);
            p += snprintf(p, end - p, "*");
            break;
        case Tyslice:
            p += tybfmt(p, end - p, t->sub[0]);
            p += snprintf(p, end - p, "[,]");
            break;
        case Tyarray:
            p += tybfmt(p, end - p, t->sub[0]);
            p += snprintf(p, end - p, "[LEN]");
            break;
        case Tyfunc:
            p += snprintf(p, end - p, "(");
            for (i = 1; i < t->nsub; i++) {
                p += snprintf(p, end - p, "%s", sep);
                p += tybfmt(p, end - p, t->sub[i]);
                sep = ", ";
            }
            p += snprintf(p, end - p, " -> ");
            p += tybfmt(p, end - p, t->sub[0]);
            p += snprintf(p, end - p, ")");
            break;
        case Tytuple:
            p += snprintf(p, end - p, "[");
            for (i = 1; i < t->nsub; i++) {
                p += snprintf(p, end - p, "%s", sep);
                p += tybfmt(p, end - p, t->sub[i]);
                sep = ", ";
            }
            p += snprintf(p, end - p, "]");
            break;
        case Tyvar:
            p += snprintf(p, end - p, "@$%d", t->tid);
            if (t->nsub) {
                p += snprintf(p, end - p, "(");
                for (i = 1; i < t->nsub; i++) {
                    p += snprintf(p, end - p, "%s", sep);
                    p += tybfmt(p, end - p, t->sub[i]);
                    sep = ", ";
                }
                p += snprintf(p, end - p, ")[]");
            }
            break;
        case Typaram:
            p += snprintf(p, end - p, "@%s", t->pname);
            break;
        case Tyname:
            p += snprintf(p, end - p, "?"); /* indicate unresolved name. should not be seen by user. */
            p += namefmt(p, end - p, t->name);
            break;
        case Tystruct:  p += fmtstruct(p, end - p, t);  break;
        case Tyunion:   p += fmtunion(p, end - p, t);   break;
        case Tyenum:    p += fmtenum(p, end - p, t);    break;
        case Ntypes:
            die("Ntypes is not a type");
            break;
    }

    /* we only show constraints on non-builtin typarams */
    if (t->type == Tyvar || t->type == Typaram)
        p += cstrfmt(p, end - p, t);

    return p - buf;
}

char *tyfmt(char *buf, size_t len, Type *t)
{
    tybfmt(buf, len, t);
    return buf;
}

char *tystr(Type *t)
{
    char buf[1024];
    tyfmt(buf, 1024, t);
    return strdup(buf);
}

void tyinit(Stab *st)
{
    int i;
    Type *ty;

#define Tc(c, n) \
    mkcstr(-1, n, NULL, 0, NULL, 0);
#include "cstr.def"
#undef Tc

    /* bool :: tctest */
    tycstrs[Tybool][0] = cstrtab[Tctest];

    /* <integer types> :: tcnum, tcint, tctest */
    for (i = Tyint8; i < Tyfloat32; i++) {
        tycstrs[i][0] = cstrtab[Tcnum];
        tycstrs[i][1] = cstrtab[Tcint];
        tycstrs[i][2] = cstrtab[Tctest];
    }

    /* <floats> :: tcnum */
    tycstrs[Tyfloat32][0] = cstrtab[Tcnum];
    tycstrs[Tyfloat64][1] = cstrtab[Tcnum];

    /* @a* :: tctest[0] = tcslice */
    tycstrs[Typtr][0] = cstrtab[Tctest];
    tycstrs[Typtr][1] = cstrtab[Tcslice];

    /* @a[,] :: tctest[0] = tcslice[0] = tcidx */
    tycstrs[Tyslice][0] = cstrtab[Tctest];
    tycstrs[Tyslice][1] = cstrtab[Tcslice];
    tycstrs[Tyslice][2] = cstrtab[Tcidx];

    /* enum :: tcint, tcnum */
    tycstrs[Tyenum][0] = cstrtab[Tcint];
    tycstrs[Tyenum][1] = cstrtab[Tcnum];

    /* array :: tcidx, tcslice */
    tycstrs[Tyarray][0] = cstrtab[Tcidx];
    tycstrs[Tyarray][1] = cstrtab[Tcslice];

    /* ptr :: tcslice, tctest */
    tycstrs[Typtr][0] = cstrtab[Tcidx];
    tycstrs[Typtr][1] = cstrtab[Tctest];

    /* slice :: tcidx, tcslice, tctest */
    tycstrs[Tyslice][0] = cstrtab[Tcidx];
    tycstrs[Tyslice][1] = cstrtab[Tcslice];
    tycstrs[Tyslice][1] = cstrtab[Tctest];

/* Definining and registering the types has to go after we define the
 * constraints, otherwise they will have no constraints set on them. */
#define Ty(t, n) \
    if (t != Ntypes) {\
      ty = mkty(-1, t); \
      if (n) { \
          puttype(st, mkname(-1, n), ty); \
      } \
    }
#include "types.def"
#undef Ty

}
