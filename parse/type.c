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

Type *littypes[Nlit] = {NULL,};
Type **tytab = NULL;
int ntypes;
Cstr **cstrtab;
int ncstrs;

static Typename typenames[] = {
    {Tyvoid, "void"},
    {Tychar, "char"},
    {Tybyte, "byte"},
    {Tyint8, "int8"},
    {Tyint16, "int16"},
    {Tyint32, "int32"},
    {Tyint64, "int64"},
    {Tyuint8, "uint8"},
    {Tyuint16, "uint16"},
    {Tyuint32, "uint32"},
    {Tyuint64, "uint64"},
    {Tyfloat32, "float32"},
    {Tyfloat64, "float64"},
    {Tybad, NULL}
};

static Cstr *tycstrs[Ntypes][4];

Type *mkty(int line, Ty ty)
{
    Type *t;
    int i;

    t = zalloc(sizeof(Type));
    t->type = ty;
    t->tid = ntypes++;
    tytab = xrealloc(tytab, ntypes*sizeof(Type*));

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

    t = mkty(line, Tyvar);
    t->pname = strdup(name);
    return t;
}

Type *mktynamed(int line, Node *name)
{
    int i;
    Type *t;

    /* is it a built in type? */
    if (name->name.nparts == 1)
        for (i = 0; typenames[i].name; i++)
            if (!strcmp(typenames[i].name, name->name.parts[0]))
                return mkty(line, typenames[i].ty);

    /* if not, resolve it in the type inference stage */
    t = mkty(line, Tyname);
    t->name = name;
    return t;
}

Type *mktyarray(int line, Type *base, Node *sz)
{
    Type *t;

    t = mkty(line, Tyarray);
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    t->asize = sz;

    return t;
}

Type *mktyslice(int line, Type *base)
{
    Type *t;

    t = mkty(line, Tyslice);
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktyptr(int line, Type *base)
{
    Type *t;

    t = mkty(line, Typtr);
    t->sub = xalloc(sizeof(Type*));
    t->sub[0] = base;
    return t;
}

Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret)
{
    Type *t;
    int i;

    t = mkty(line, Tyfunc);
    t->nsub = nargs + 1;
    t->sub = xalloc((1 + nargs)*sizeof(Type));
    t->sub[0] = ret;
    for (i = 0; i < nargs; i++)
        t->sub[i + 1] = decltype(args[i]);
    return t;
}

Type *mktystruct(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tystruct);
    t->nsub = ndecls;
    t->sdecls = memdup(decls, ndecls*sizeof(Node *));
    return t;
}

Type *mktyunion(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tyunion);
    t->udecls = decls;
    return t;
}

Type *mktyenum(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mkty(line, Tyenum);
    t->edecls = decls;
    return t;
}

void tlappend(Type ***tl, int *len, Type *t)
{
    *tl = xrealloc(tl, (*len + 1)*sizeof(Type*));
    (*tl)[*len] = t;
    (*len)++;
}

static int namefmt(char *buf, size_t len, Node *name)
{
    int i;
    char *p;
    char *end;
    char *sep;

    p = buf;
    end = p + len;
    sep = "";
    for (i = 0; i < name->name.nparts; i++) {
        p += snprintf(p, end - p, "%s%s", sep, name->name.parts[i]);
        sep = ".";
    }
    return len - (end - p);
}

int constrain(Type *t, Cstr *c)
{
    if (!t->cstrs)
        t->cstrs = mkbs();
    if (t->type != Tyvar && t->type != Typaram)
        return 0;
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
    int first;
    int i;

    if (!t->cstrs || !bscount(t->cstrs))
        return 0;

    p = buf;
    end = p + len;
    first = 1;

    p += snprintf(p, end - p, " :: ");
    for (i = 0; i < ncstrs; i++) {
        if (bshas(t->cstrs, i)) {
            if (!first) {
                first = 0;
                p += snprintf(p, end - p, ", ");
            }
            p += snprintf(p, end - p, "%s", cstrtab[i]->name);
        }
    }
    return end - p;
}

static int tybfmt(char *buf, size_t len, Type *t)
{
    char *p;
    char *end;
    int i;
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
            break;
        case Typaram:
            p += snprintf(p, end - p, "@%s", t->pname);
            break;
        case Tyname:
            p += snprintf(p, end - p, "?"); /* indicate unresolved name. should not be seen by user. */
            p += namefmt(p, end - p, t->name);
            break;
        case Tystruct:
        case Tyunion:
        case Tyenum:
            snprintf(p, end - p, "TYPE ?");
            break;
        case Ntypes:
            die("Ntypes is not a type");
            break;
    }

    /* we only show constraints on non-builtin typarams */
    if (t->type == Tyvar || t->type == Typaram)
        p += cstrfmt(p, end - p, t);

    return len - (end - p);
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

static struct {
    char enc;    /* character to encode */
    int special; /* 0 => atomic, 1 => unary, 2 => nary, -1 => special-cased */
} enctab[] = {
    [Tybad]     = {'\0',0},
    [Tyvoid]    = {'z', 0},

    [Tybool]    = {'t', 0},
    [Tychar]    = {'c', 0},

    [Tyint8]    = {'h', 0},
    [Tyint16]   = {'s', 0},
    [Tyint]     = {'i', 0},
    [Tyint32]   = {'l', 0},
    [Tyint64]   = {'q', 0},
    [Tylong]    = {'v', 0},

    [Tybyte]    = {'b', 0},
    [Tyuint8]   = {'H', 0},
    [Tyuint16]  = {'S', 0},
    [Tyuint]    = {'I', 0},
    [Tyuint32]  = {'L', 0},
    [Tyuint64]  = {'Q', 0},
    [Tyulong]   = {'V', 0},

    [Tyfloat32] = {'f', 0},
    [Tyfloat64] = {'F', 0},

    [Tyvalist]  = {'.', 1},
    [Typtr]     = {'*', 1},
    [Tyslice]   = {':', 1},
    [Tyarray]   = {'$', 1},
    [Tyfunc]    = {'<', 2},
    [Tytuple]   = {',', 2},
    [Tyvar]     = {'#', -1},
    [Typaram]   = {'@', -1},
    [Tyname]    = {'%', -1},
    [Tystruct]  = {'^', -1},
    [Tyunion]   = {'!', -1},
    [Tyenum]    = {'/', -1},
    [Ntypes]    = {'\0', 0}
};

static int encbuf(Type *t, char *buf, size_t len)
{
    char *p;
    char *end;
    int i;

    p = buf;
    end = buf + len;

    if (len <= 0)
        return 0;
    *p++ = enctab[t->type].enc;
    if (enctab[t->type].special == 1) {
        encbuf(t->sub[0], p, end - p);
    } else if (enctab[t->type].special == 2) {
        p += snprintf(p, end - p, "%zd", t->nsub);
        for (i = 0; i < t->nsub; i++)
            encbuf(t->sub[i], p, end - p);
    } else if (enctab[t->type].special == -1) {
        switch (t->type) {
            case Tyname:  p += namefmt(p, end - p, t->name); break;
            case Tyvar:   die("Tyvar is not encodable");  break;
            case Typaram: p += snprintf(p, end - p, "%s;", t->pname); break;
            case Tystruct:
            case Tyunion:
            case Tyenum:
            default:
                die("type %s should not be special", tystr(t));
        }
        /* all special forms end with ';' */
        snprintf(p, end - p, ";");
    } else {
        die("Don't know how to encode %s", tystr(t));
    }
    return p - buf;
}

char *tyenc(Type *t)
{
    char buf[1024];

    encbuf(t, buf, 1024);
    return strdup(buf);
}

static Type *decname(char *p)
{
    char *name;
    char *parts[16];
    char *startp;
    int i;
    Node *n;

    i = 0;
    name = p;
    startp = p;
    while (1) {
        if (*p == '.' || *p == ';') {
            if (i == 16)
                die("too many parts to name %s", name);
            parts[i++] = strdupn(startp, p - startp);
            startp = p;
        }
        if (!*p)
            die("bad decoded name %s", name);
        if (*p == ';')
            break;
        if (*p == '.')
            p++;
        p++;
    }

    n = mkname(-1, parts[i - 1]);
    i--;
    for (; i > 0; i--)
        setns(n, parts[i - 1]);
    return mktynamed(-1, n);
}

Type *tydec(char *p)
{
    Ty i;
    Type *t;

    for (i = 0; i < Ntypes; i++) {
        if (enctab[i].enc == *p)
            break;
    }
    p++;
    if (enctab[i].special == 0) {
        t = mkty(-1, i);
    } else if (enctab[i].special == 1) {
        t = mkty(-1, i);
        t->nsub = 1;
        t->sub = xalloc(sizeof(Type*));
        t->sub[0] = tydec(p);
    } else {
        switch (i) {
            case Tyname: t = decname(p); break;
            default:
                die("Unimplemented tydec for %s", p);
                break;
        }
    }
    return t;
}

void tyinit(void)
{
    int i;

#define Tc(c, n) \
    mkcstr(-1, n, NULL, 0, NULL, 0);
#include "cstr.def"
#undef Tc

#define Ty(t) \
    mkty(-1, t);
#include "types.def"
#undef Ty

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
}
