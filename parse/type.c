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

Typename typenames[] = {
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


static int nexttid = 0;
static Type *mktype(Ty ty)
{
    Type *t;

    t = xalloc(sizeof(Type));
    t->type = ty;
    t->tid = nexttid++;
    return t;
}

Type *mktyvar(int line)
{
    Type *t;

    t = mktype(Tyvar);
    return t;
}

Type *mktyparam(int line, char *name)
{
    Type *t;

    t = mktype(Tyvar);
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
                return mktype(typenames[i].ty);

    /* if not, resolve it in the type inference stage */
    t = mktype(Tyname);
    t->name = name;
    return t;
}

Type *mktyarray(int line, Type *base, Node *sz)
{
    Type *t;

    t = mktype(Tyarray);
    t->abase = base;
    t->asize = sz;

    return t;
}

Type *mktyslice(int line, Type *base)
{
    Type *t;

    t = mktype(Tyslice);
    t->sbase = base;
    return t;
}

Type *mktyptr(int line, Type *base)
{
    Type *t;

    t = mktype(Typtr);
    t->pbase = base;
    return t;
}

Type *mktyfunc(int line, Node **args, size_t nargs, Type *ret)
{
    Type *t;
    Type **sub;

    t = mktype(Tyfunc);
    sub = xalloc((1 + nargs)*sizeof(Type));
    sub[0] = ret;
    die("pull out subtypes for fn");
    t->fnsub = sub;
    return t;
}

Type *mktystruct(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mktype(Tystruct);
    t->sdecls = decls;
    return t;
}

Type *mktyunion(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mktype(Tyunion);
    t->udecls = decls;
    return t;
}

Type *mktyenum(int line, Node **decls, size_t ndecls)
{
    Type *t;

    t = mktype(Tyenum);
    t->edecls = decls;
    return t;
}

void tlappend(Type ***tl, int *len, Type *t)
{
    *tl = xrealloc(tl, (*len + 1)*sizeof(Type*));
    (*tl)[*len] = t;
    (*len)++;
}

int tybfmt(char *buf, size_t len, Type *t)
{
    size_t n;
    char *p;

    n = 0;
    p = buf;
    switch (t->type) {
        case Tybad:     n += snprintf(p, len - n, "BAD");       break;
        case Tyvoid:    n += snprintf(p, len - n, "void");      break;
        case Tybool:    n += snprintf(p, len - n, "bool");      break;
        case Tychar:    n += snprintf(p, len - n, "char");      break;
        case Tyint8:    n += snprintf(p, len - n, "int8");      break;
        case Tyint16:   n += snprintf(p, len - n, "int16");     break;
        case Tyint:     n += snprintf(p, len - n, "int");       break;
        case Tyint32:   n += snprintf(p, len - n, "int32");     break;
        case Tyint64:   n += snprintf(p, len - n, "int64");     break;
        case Tylong:    n += snprintf(p, len - n, "long");      break;
        case Tybyte:    n += snprintf(p, len - n, "byte");      break;
        case Tyuint8:   n += snprintf(p, len - n, "uint8");     break;
        case Tyuint16:  n += snprintf(p, len - n, "uint16");    break;
        case Tyuint:    n += snprintf(p, len - n, "uint");      break;
        case Tyuint32:  n += snprintf(p, len - n, "uint32");    break;
        case Tyuint64:  n += snprintf(p, len - n, "uint64");    break;
        case Tyulong:   n += snprintf(p, len - n, "ulong");     break;
        case Tyfloat32: n += snprintf(p, len - n, "float32");   break;
        case Tyfloat64: n += snprintf(p, len - n, "float64");   break;
        case Tyvalist:  n += snprintf(p, len - n, "...");       break;

        case Typtr:     
            n += tybfmt(p, len - n, t->pbase);
            n += snprintf(p, len - n, "*");
            break;
        case Tyslice:
            n += tybfmt(p, len - n, t->sbase);
            p = &buf[n];
            n += snprintf(p, len - n, "[,]");
            break;
        case Tyarray:
            n += tybfmt(p, len - n, t->abase);
            p = &buf[n];
            n += snprintf(p, len - n, "[LEN]");
            break;
        case Tyfunc:
        case Tytuple:
        case Tyvar:
        case Typaram:
        case Tyname:
        case Tystruct:
        case Tyunion:
        case Tyenum:
            snprintf(p, len - n, "TYPE ?");
            break;
    }

    return n;
}

char *tyfmt(char *buf, size_t len, Type *t)
{
    tyfmt(buf, len, t);
    return buf;
}

char *tystr(Type *t)
{
    char buf[1024];
    tyfmt(buf, 1024, t);
    return strdup(buf);
}
