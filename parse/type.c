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
size_t ntypes;
Cstr **cstrtab;
size_t ncstrs;

/* Built in type constraints */
static Cstr *tycstrs[Ntypes + 1][4];

Type *mktype(int line, Ty ty)
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
        setcstr(t, tycstrs[ty][i]);

    return t;
}

/*
 * Duplicates a type, so we can frob
 * its internals later
 */
Type *tydup(Type *t)
{
    Type *r;

    r = mktype(t->line, t->type);
    r->resolved = 0; /* re-resolving doesn't hurt */
    r->cstrs = bsdup(t->cstrs);
    r->nsub = t->nsub;
    r->nmemb = t->nmemb;
    r->sub = memdup(t->sub, t->nsub * sizeof(Type*));
    switch (t->type) {
        case Tyunres:    r->name = t->name;              break;
        case Tyarray:   r->asize = t->asize;            break;
        case Typaram:   r->pname = strdup(t->pname);    break;
        case Tystruct:  r->sdecls = memdup(t->sdecls, t->nmemb*sizeof(Node*));   break;
        case Tyunion:   r->udecls = memdup(t->udecls, t->nmemb*sizeof(Node*));   break;
        default:        break;
    }
    return r;
}

/*
 * Creates a Tyvar with the same
 * constrants as the 'like' type
 */
Type *mktylike(int line, Ty like)
{
    Type *t;
    int i;

    t = mktyvar(line);
    for (i = 0; tycstrs[like][i]; i++)
        setcstr(t, tycstrs[like][i]);
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

    t = mktype(line, Tyvar);
    return t;
}

Type *mktyparam(int line, char *name)
{
    Type *t;

    t = mktype(line, Typaram);
    t->pname = strdup(name);
    return t;
}

Type *mktyunres(int line, Node *name)
{
    Type *t;

    /* resolve it in the type inference stage */
    t = mktype(line, Tyunres);
    t->name = name;
    return t;
}

Type *mktytmpl(int line, Node *name, Type **param, size_t nparam, Type *base)
{
    Type *t;

    t = mktype(line, Tyname);
    t->name = name;
    t->nsub = 1;
    t->cstrs = bsdup(base->cstrs);
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    t->param = param;
    t->nparam = nparam;
    return t;
}

Type *mktyname(int line, Node *name, Type *base)
{
    return mktytmpl(line, name, NULL, 0, base);
}

Type *mktyarray(int line, Type *base, Node *sz)
{
    Type *t;

    t = mktype(line, Tyarray);
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

    t = mktype(line, Tyslice);
    t->nsub = 1;
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktyidxhack(int line, Type *base)
{
    Type *t;

    t = mktype(line, Tyvar);
    t->nsub = 1;
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktyptr(int line, Type *base)
{
    Type *t;

    t = mktype(line, Typtr);
    t->nsub = 1;
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktytuple(int line, Type **sub, size_t nsub)
{
    Type *t;
    size_t i;

    t = mktype(line, Tytuple);
    t->nsub = nsub;
    t->sub = xalloc(nsub*sizeof(Type));
    for (i = 0; i < nsub; i++)
        t->sub[i] = sub[i];
    return t;
}

Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret)
{
    Type *t;
    size_t i;

    t = mktype(line, Tyfunc);
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

    t = mktype(line, Tystruct);
    t->nsub = 0;
    t->nmemb = ndecls;
    t->sdecls = memdup(decls, ndecls*sizeof(Node *));
    return t;
}

Type *mktyunion(int line, Ucon **decls, size_t ndecls)
{
    Type *t;

    t = mktype(line, Tyunion);
    t->nmemb = ndecls;
    t->udecls = decls;
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

Type *tybase(Type *t)
{
    assert(t != NULL);
    while (t->type == Tyname)
        t = t->sub[0];
    return t;
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

int setcstr(Type *t, Cstr *c)
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

int cstrfmt(char *buf, size_t len, Type *t)
{
    size_t i;
    char *p;
    char *end;
    char *sep;

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
        name = declname(t->sdecls[i]);
        ty = tystr(decltype(t->sdecls[i]));
        p += snprintf(p, end - p, "%s:%s; ", name, ty);
        free(ty);
    }
    p += snprintf(p, end - p, ";;");
    return p - buf;
}

static int fmtunion(char *buf, size_t len, Type *t)
{
    size_t i;
    char *end, *p;
    char *name, *ty;

    p = buf;
    end = p + len;
    p += snprintf(p, end - p, "union ");
    for (i = 0; i < t->nmemb; i++) {
        name = namestr(t->udecls[i]->name);
        ty = tystr(t->udecls[i]->etype);
        p += snprintf(p, end - p, "`%s %s; ", name, ty);
        free(ty);
    }
    p += snprintf(p, end - p, ";;");
    return p - buf;
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
            for (i = 0; i < t->nsub; i++) {
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
                for (i = 0; i < t->nsub; i++) {
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
        case Tyunres:
            p += snprintf(p, end - p, "?"); /* indicate unresolved name. should not be seen by user. */
            p += namefmt(p, end - p, t->name);
            break;
        case Tyname:  
            p += snprintf(p, end - p, "%s", namestr(t->name));
            if (t->nparam) {
                p += snprintf(p, end - p, "(");
                for (i = 0; i < t->nparam; i++)  {
                    p += snprintf(p, end - p, "%s", sep);
                    p += tybfmt(p, end - p, t->param[i]);
                    sep = ", ";
                }
                p += snprintf(p, end - p, ")");
            }
            break;
        case Tystruct:  p += fmtstruct(p, end - p, t);  break;
        case Tyunion:   p += fmtunion(p, end - p, t);   break;
        case Ntypes:
            die("Ntypes is not a type");
            break;
    }

    /* we only show constraints on non-builtin typaram */
    if (t->type == Tyvar || t->type == Typaram)
        p += cstrfmt(p, end - p, t);

    return p - buf;
}

char *tyfmt(char *buf, size_t len, Type *t)
{
    tybfmt(buf, len, t);
    return buf;
}

char *cstrstr(Type *t)
{
    char buf[1024];
    cstrfmt(buf, 1024, t);
    return strdup(buf);
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

    tycstrs[Tychar][0] = cstrtab[Tcnum];
    tycstrs[Tychar][1] = cstrtab[Tcint];
    tycstrs[Tychar][2] = cstrtab[Tctest];

    tycstrs[Tybyte][0] = cstrtab[Tcnum];
    tycstrs[Tybyte][1] = cstrtab[Tcint];
    tycstrs[Tybyte][2] = cstrtab[Tctest];

    /* <integer types> :: tcnum, tcint, tctest */
    for (i = Tyint8; i < Tyfloat32; i++) {
        tycstrs[i][0] = cstrtab[Tcnum];
        tycstrs[i][1] = cstrtab[Tcint];
        tycstrs[i][2] = cstrtab[Tctest];
    }

    /* <floats> :: tcnum */
    tycstrs[Tyfloat32][0] = cstrtab[Tcnum];
    tycstrs[Tyfloat32][1] = cstrtab[Tcfloat];
    tycstrs[Tyfloat64][0] = cstrtab[Tcnum];
    tycstrs[Tyfloat64][1] = cstrtab[Tcfloat];

    /* @a* :: tctest[0] = tcslice */
    tycstrs[Typtr][0] = cstrtab[Tctest];
    tycstrs[Typtr][1] = cstrtab[Tcslice];

    /* @a[,] :: tctest[0] = tcslice[0] = tcidx */
    tycstrs[Tyslice][0] = cstrtab[Tctest];
    tycstrs[Tyslice][1] = cstrtab[Tcslice];
    tycstrs[Tyslice][2] = cstrtab[Tcidx];

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
      ty = mktype(-1, t); \
      if (n) { \
          puttype(st, mkname(-1, n), ty); \
      } \
    }
#include "types.def"
#undef Ty

}
