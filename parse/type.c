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
Type **types = NULL;
size_t ntypes;
Trait **traittab;
size_t ntraits;

/* Built in type constraints */
static Trait *traits[Ntypes + 1][4];

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
    types = xrealloc(types, ntypes*sizeof(Type*));
    types[t->tid] = t;
    if (ty <= Tyvalist) /* the last builtin atomic type */
        t->vis = Visbuiltin;

    for(i = 0; traits[ty][i]; i++)
        settrait(t, traits[ty][i]);

    return t;
}

/*
 * Shallowly duplicates a type, so we can frob
 * its internals later
 */
Type *tydup(Type *t)
{
    Type *r;

    r = mktype(t->line, t->type);
    r->resolved = 0; /* re-resolving doesn't hurt */
    r->fixed = 0; /* re-resolving doesn't hurt */

    r->traits = bsdup(t->traits);
    r->traitlist = memdup(t->traitlist, t->ntraitlist * sizeof(Node*));
    r->ntraitlist = t->ntraitlist;

    r->arg = memdup(t->arg, t->narg * sizeof(Type*));
    r->narg = t->narg;
    r->inst = memdup(t->arg, t->narg * sizeof(Type*));
    r->ninst = t->ninst;

    r->sub = memdup(t->sub, t->nsub * sizeof(Type*));
    r->nsub = t->nsub;
    r->nmemb = t->nmemb;
    switch (t->type) {
        case Tyname:   	r->name = t->name;              break;
        case Tyunres:   r->name = t->name;              break;
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
    for (i = 0; traits[like][i]; i++)
        settrait(t, traits[like][i]);
    return t;
}

/* steals memb, funcs */
Trait *mktrait(int line, char *name, Node **memb, size_t nmemb, Node **funcs, size_t nfuncs)
{
    Trait *c;

    c = zalloc(sizeof(Trait));
    c->name = strdup(name);
    c->memb = memb;
    c->nmemb = nmemb;
    c->funcs = funcs;
    c->nfuncs = nfuncs;
    c->cid = ntraits++;

    traittab = xrealloc(traittab, ntraits*sizeof(Trait*));
    traittab[c->cid] = c;
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

Type *mktyunres(int line, Node *name, Type **arg, size_t narg)
{
    Type *t;

    /* resolve it in the type inference stage */
    t = mktype(line, Tyunres);
    t->name = name;
    t->arg = arg;
    t->narg = narg;
    return t;
}

Type *mktyname(int line, Node *name, Type **param, size_t nparam, Type *base)
{
    Type *t;

    t = mktype(line, Tyname);
    t->name = name;
    t->nsub = 1;
    t->traits = bsdup(base->traits);
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    t->param = param;
    t->nparam = nparam;
    return t;
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
    switch (tybase(t)->type) {
        case Tyint8: case Tyint16: case Tyint:
        case Tyint32: case Tyint64: case Tylong:
            return 1;
        default:
            return 0;
    }
}

int istyfloat(Type *t)
{
    switch (tybase(t)->type) {
        case Tyfloat32: case Tyfloat64:
            return 1;
        default:
            return 0;
    }
}

int isgeneric(Type *t)
{
    size_t i;

    if (t->type != Tyname && t->type != Tyunres)
        return 0;
    if (!t->narg)
        return t->nparam > 0;
    else
        for (i = 0; i < t->narg; i++)
            if (hasparams(t->arg[i]))
                return 1;
    return 0;
}

/*
 * Checks if a type contains any type
 * parameers at all (ie, if it generic).
 */
int hasparams(Type *t)
{
    size_t i;

    if (t->type == Typaram || isgeneric(t))
        return 1;
    for (i = 0; i < t->nsub; i++)
        if (hasparams(t->sub[i]))
            return 1;
    for (i = 0; i < t->narg; i++)
        if (hasparams(t->arg[i]))
            return 1;
    return 0;
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

int settrait(Type *t, Trait *c)
{
    if (!t->traits)
        t->traits = mkbs();
    bsput(t->traits, c->cid);
    return 1;
}

int hastrait(Type *t, Trait *c)
{
    return t->traits && bshas(t->traits, c->cid);
}

int traitfmt(char *buf, size_t len, Type *t)
{
    size_t i;
    char *p;
    char *end;
    char *sep;

    if (!t->traits || !bscount(t->traits))
        return 0;

    p = buf;
    end = p + len;

    p += snprintf(p, end - p, " :: ");
    sep = "";
    for (i = 0; i < ntraits; i++) {
        if (bshas(t->traits, i)) {
            p += snprintf(p, end - p, "%s%s", sep, traittab[i]->name);
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
    size_t narg;
    Type **arg;

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
            p += snprintf(p, end - p, "#");
            break;
        case Tyslice:
            p += tybfmt(p, end - p, t->sub[0]);
            p += snprintf(p, end - p, "[:]");
            break;
        case Tyarray:
            p += tybfmt(p, end - p, t->sub[0]);
            p += snprintf(p, end - p, "[%llu]", t->asize->expr.args[0]->lit.intval);
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
            p += snprintf(p, end - p, "$%d", t->tid);
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
            if (t->narg) {
                p += snprintf(p, end - p, "(");
                for (i = 0; i < t->narg; i++)  {
                    p += snprintf(p, end - p, "%s", sep);
                    p += tybfmt(p, end - p, t->arg[i]);
                    sep = ", ";
                }
                p += snprintf(p, end - p, ")");
            }
            break;
        case Tyname:
            if (t->name->name.ns)
                p += snprintf(p, end - p, "%s.", t->name->name.ns);
            p += snprintf(p, end - p, "%s", namestr(t->name));
            if (t->narg) {
                arg = t->arg;
                narg = t->narg;
            } else {
                arg = t->param;
                narg = t->nparam;
            }
            if (!narg)
                break;
            p += snprintf(p, end - p, "(");
            for (i = 0; i < narg; i++)  {
                p += snprintf(p, end - p, "%s", sep);
                p += tybfmt(p, end - p, arg[i]);
                sep = ", ";
            }
            p += snprintf(p, end - p, ")");
            break;
        case Tystruct:  p += fmtstruct(p, end - p, t);  break;
        case Tyunion:   p += fmtunion(p, end - p, t);   break;
        case Ntypes:
            die("Ntypes is not a type");
            break;
    }

    /* we only show constraints on non-builtin typaram */
    if (t->type == Tyvar || t->type == Typaram)
        p += traitfmt(p, end - p, t);

    return p - buf;
}

char *tyfmt(char *buf, size_t len, Type *t)
{
    tybfmt(buf, len, t);
    return buf;
}

char *traitstr(Type *t)
{
    char buf[1024];
    traitfmt(buf, 1024, t);
    return strdup(buf);
}

char *tystr(Type *t)
{
    char buf[1024];
    tyfmt(buf, 1024, t);
    return strdup(buf);
}

ulong tyhash(void *ty)
{
    size_t i;
    Type *t;
    ulong hash;

    t = (Type *)ty;
    if (t->type == Typaram)
        hash = strhash(t->pname);
    else
        hash = inthash(t->tid);

    for (i = 0; i < t->narg; i++)
        hash ^= tyhash(t->arg[i]);
    return hash;
}

int tyeq(void *t1, void *t2)
{
    Type *a, *b;
    size_t i;

    a = (Type *)t1;
    b = (Type *)t2;
    if (a == b)
        return 1;
    if (a->type != b->type)
        return 0;
    if (a->tid == b->tid)
        return 1;
    if (a->narg != b->narg)
        return 0;
    if (a->nsub != b->nsub)
        return 0;
    if (a->nmemb != b->nmemb)
        return 0;
    switch (a->type) {
        case Typaram:
            return streq(a->pname, b->pname);
            break;
        case Tyunion:
            for (i = 0; i < a->nmemb; i++)
                if (!tyeq(a->udecls[i]->etype, b->udecls[i]->etype))
                    return 0;
            break;
        case Tyname:
            if (!nameeq(a->name, b->name))
                return 0;
            for (i = 0; i < a->narg; i++)
                if (!tyeq(a->arg[i], b->arg[i]))
                    return 0;
            for (i = 0; i < a->nsub; i++)
                if (!tyeq(a->sub[i], b->sub[i]))
                    return 0;
            break;
        default:
            for (i = 0; i < a->nsub; i++)
                if (!tyeq(a->sub[i], b->sub[i]))
                    return 0;
            break;
    }
    return 1;
}

void tyinit(Stab *st)
{
    int i;
    Type *ty;

#define Tc(c, n) \
    mktrait(-1, n, NULL, 0, NULL, 0);
#include "trait.def"
#undef Tc

    /* bool :: tctest */
    traits[Tybool][0] = traittab[Tctest];

    traits[Tychar][0] = traittab[Tcnum];
    traits[Tychar][1] = traittab[Tcint];
    traits[Tychar][2] = traittab[Tctest];

    traits[Tybyte][0] = traittab[Tcnum];
    traits[Tybyte][1] = traittab[Tcint];
    traits[Tybyte][2] = traittab[Tctest];

    /* <integer types> :: tcnum, tcint, tctest */
    for (i = Tyint8; i < Tyfloat32; i++) {
        traits[i][0] = traittab[Tcnum];
        traits[i][1] = traittab[Tcint];
        traits[i][2] = traittab[Tctest];
    }

    /* <floats> :: tcnum */
    traits[Tyfloat32][0] = traittab[Tcnum];
    traits[Tyfloat32][1] = traittab[Tcfloat];
    traits[Tyfloat64][0] = traittab[Tcnum];
    traits[Tyfloat64][1] = traittab[Tcfloat];

    /* @a* :: tctest[0] = tcslice */
    traits[Typtr][0] = traittab[Tctest];
    traits[Typtr][1] = traittab[Tcslice];

    /* @a[,] :: tctest[0] = tcslice[0] = tcidx */
    traits[Tyslice][0] = traittab[Tctest];
    traits[Tyslice][1] = traittab[Tcslice];
    traits[Tyslice][2] = traittab[Tcidx];

    /* array :: tcidx, tcslice */
    traits[Tyarray][0] = traittab[Tcidx];
    traits[Tyarray][1] = traittab[Tcslice];

    /* ptr :: tcslice, tctest */
    traits[Typtr][0] = traittab[Tcidx];
    traits[Typtr][1] = traittab[Tctest];

    /* slice :: tcidx, tcslice, tctest */
    traits[Tyslice][0] = traittab[Tcidx];
    traits[Tyslice][1] = traittab[Tcslice];
    traits[Tyslice][1] = traittab[Tctest];

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
