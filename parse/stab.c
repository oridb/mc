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

typedef struct Tydefn Tydefn;
struct Tydefn {
    int line;
    Node *name;
    Type *type;
};

#define Maxstabdepth 128
static Stab *stabstk[Maxstabdepth];
static int stabstkoff;
Stab *curstab()
{
    return stabstk[stabstkoff - 1];
}

void pushstab(Stab *st)
{
    stabstk[stabstkoff++] = st;
}

void popstab(void)
{
    stabstkoff--;
}


static char *name(Node *n)
{
    return n->name.parts[n->name.nparts - 1];
}

static ulong namehash(void *n)
{
    return strhash(name(n));
}

static int nameeq(void *a, void *b)
{
    return a == b || !strcmp(name(a), name(b));
}

Sym *mksym(int line, Node *name, Type *ty)
{
    Sym *sym;

    sym = zalloc(sizeof(Sym));
    sym->name = name;
    sym->type = ty;
    sym->line = line;
    return sym;
}

Stab *mkstab(Stab *super)
{
    Stab *st;

    st = zalloc(sizeof(Stab));
    st->super = super;
    st->ns = mkht(namehash, nameeq);
    st->dcl = mkht(namehash, nameeq);
    st->ty = mkht(namehash, nameeq);
    return st;
}

/* FIXME: do namespaces */
Sym *getdcl(Stab *st, Node *n)
{
    Sym *s;
    do {
        if ((s = htget(st->dcl, n)))
            return s;
    } while (st->super);
    return NULL;
}

Type *gettype(Stab *st, Node *n)
{
    Tydefn *t;
    
    do {
        if ((t = htget(st->ty, n)))
            return t->type;
    } while (st->super);
    return NULL;
}

Stab *getstab(Stab *st, Node *n)
{
    Stab *s;
    do {
        if ((s = htget(st->ns, n)))
            return s;
    } while (st->super);
    return NULL;
}

void putdcl(Stab *st, Sym *s)
{
    Sym *d;

    d = getdcl(st, s->name);
    if (d)
        fatal(s->line, "%s already declared (line %d", name(s->name), d->line);
    htput(st->dcl, s->name, s);
}

void puttype(Stab *st, Node *n, Type *t)
{
    Type *ty;
    Tydefn *td;

    ty = gettype(st, n);
    if (ty)
        fatal(n->line, "Type %s already defined", name(n));
    td = xalloc(sizeof(Tydefn));
    td->line = n->line;
    td->name = n;
    td->type = t;
    htput(st->ty, td->name, td);
}

void putns(Stab *st, Stab *scope)
{
    Stab *s;

    s = getstab(st, scope->name);
    if (s)
        fatal(scope->name->line, "Ns %s already defined", name(s->name));
    htput(st->ns, scope->name, scope);
}

