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

/* Allows us to look up types/traits by name nodes */
typedef struct Tydefn Tydefn;
typedef struct Traitdefn Traitdefn;
struct Tydefn {
    int line;
    Node *name;
    Type *type;
};

struct Traitdefn {
    int line;
    Node *name;
    Trait *trait;
};

#define Maxstabdepth 128
static Stab *stabstk[Maxstabdepth];
int stabstkoff;

/* scope management */
Stab *curstab()
{
    assert(stabstkoff > 0);
    return stabstk[stabstkoff - 1];
}

void pushstab(Stab *st)
{
    assert(stabstkoff < Maxstabdepth);
    stabstk[stabstkoff++] = st;
}

void popstab(void)
{
    assert(stabstkoff > 0);
    stabstkoff--;
}

/* name hashing: we want namespaced lookups to find the
 * name even if we haven't set the namespace up, since
 * we can update it after the fact. */
static ulong nsnamehash(void *n)
{
    return strhash(namestr(n));
}

static int nsnameeq(void *a, void *b)
{
    return a == b || !strcmp(namestr(a), namestr(b));
}

static ulong implhash(void *p)
{
    Node *n;
    ulong h;

    n = p;
    h = nsnamehash(n->impl.traitname);
    h *= tyhash(n->impl.type);
    return h;
}

static int impleq(void *pa, void *pb)
{
    Node *a, *b;

    a = pa;
    b = pb;
    if (nsnameeq(a->impl.traitname, b->impl.traitname))
        return tyeq(a->impl.type, b->impl.type);
    return 0;
}

Stab *mkstab()
{
    Stab *st;

    st = zalloc(sizeof(Stab));
    st->ns = mkht(strhash, streq);
    st->dcl = mkht(nsnamehash, nsnameeq);
    st->ty = mkht(nsnamehash, nsnameeq);
    st->tr = mkht(nsnamehash, nsnameeq);
    st->uc = mkht(nsnamehash, nsnameeq);
    st->impl = mkht(implhash, impleq);
    return st;
}

/* 
 * Searches for declarations from current
 * scope, and all enclosing scopes. Does
 * not resolve namespaces -- that is the job
 * of the caller of this function.
 *
 * If a resoved name is not global, and is
 * not in the current scope, it is recorded
 * in the scope's closure.
 */
Node *getdcl(Stab *st, Node *n)
{
    Node *s;
    Stab *orig;

    orig = st;
    do {
        s = htget(st->dcl, n);
        if (s) {
            /* record that this is in the closure of this scope */
            if (!st->closure)
                st->closure = mkht(nsnamehash, nsnameeq);
            if (st != orig && !n->decl.isglobl)
                htput(st->closure, s->decl.name, s);
            return s;
        }
        st = st->super;
    } while (st);
    return NULL;
}

Type *gettype_l(Stab *st, Node *n)
{
    Tydefn *t;

    if ((t = htget(st->ty, n)))
        return t->type;
    return NULL;
}


Type *gettype(Stab *st, Node *n)
{
    Tydefn *t;

    do {
        if ((t = htget(st->ty, n)))
            return t->type;
        st = st->super;
    } while (st);
    return NULL;
}

Ucon *getucon(Stab *st, Node *n)
{
    Ucon *uc;

    do {
        if ((uc = htget(st->uc, n)))
            return uc;
        st = st->super;
    } while (st);
    return NULL;
}

Trait *gettrait(Stab *st, Node *n)
{
    Traitdefn *c;

    do {
        if ((c = htget(st->tr, n)))
            return c->trait;
        st = st->super;
    } while (st);
    return NULL;
}

Stab *getns_str(Stab *st, char *name)
{
    Stab *s;

    if (!strcmp(namestr(st->name), name))
        return st;
    do {
        if ((s = htget(st->ns, name)))
            return s;
        st = st->super;
    } while (st);
    return NULL;
}

Stab *getns(Stab *st, Node *n)
{
    return getns_str(st, namestr(n));
}

void putdcl(Stab *st, Node *s)
{
    Node *d;

    d = htget(st->dcl, s->decl.name);
    if (d)
        fatal(s->line, "%s already declared (on line %d)", namestr(s->decl.name), d->line);
    forcedcl(st, s);
}

void forcedcl (Stab *st, Node *s) {
    if (st->name)
        setns(s->decl.name, namestr(st->name));
    htput(st->dcl, s->decl.name, s);
    assert(htget(st->dcl, s->decl.name) != NULL);
}

void updatetype(Stab *st, Node *n, Type *t)
{
    Tydefn *td;

    td = htget(st->ty, n);
    if (!td)
        die("No type %s to update", namestr(n));
    td->type = t;
}

void puttype(Stab *st, Node *n, Type *t)
{
    Tydefn *td;

    if (gettype(st, n))
        fatal(n->line, "Type %s already defined", tystr(gettype(st, n)));
    td = xalloc(sizeof(Tydefn));
    td->line = n->line;
    td->name = n;
    td->type = t;
    if (st->name)
        setns(n, namestr(st->name));
    htput(st->ty, td->name, td);
}

void putucon(Stab *st, Ucon *uc)
{
    if (getucon(st, uc->name))
        fatal(uc->line, "union constructor %s already defined", namestr(uc->name));
    htput(st->uc, uc->name, uc);
}

void puttrait(Stab *st, Node *n, Trait *c)
{
    Traitdefn *td;

    if (gettrait(st, n))
        fatal(n->line, "Trait %s already defined", namestr(n));
    if (gettype(st, n))
        fatal(n->line, "Trait %s already defined as type", namestr(n));
    td = xalloc(sizeof(Tydefn));
    td->line = n->line;
    td->name = n;
    td->trait = c;
    htput(st->tr, td->name, td);
}

void putimpl(Stab *st, Node *n)
{
    if (hasimpl(st, n))
        fatal(n->line, "Trait %s already defined", namestr(n));
    if (st->name)
        setns(n->impl.traitname, namestr(st->name));
    htput(st->impl, n, n);
}

int hasimpl(Stab *st, Node *n)
{
    return hthas(st->impl, n);
}

void putns(Stab *st, Stab *scope)
{
    Stab *s;

    s = getns(st, scope->name);
    if (s)
        fatal(scope->name->line, "Ns %s already defined", namestr(s->name));
    htput(st->ns, namestr(scope->name), scope);
}

/*
 * Sets the namespace of a symbol table, and
 * changes the namespace of all contained symbols
 * to match it.
 */
void updatens(Stab *st, char *name)
{
    void **k;
    size_t i, nk;
    Tydefn *td;

    if (st->name)
        die("Stab %s already has namespace; Can't set to %s", namestr(st->name), name);
    st->name = mkname(-1, name);
    k = htkeys(st->dcl, &nk);
    for (i = 0; i < nk; i++)
        setns(k[i], name);
    free(k);
    k = htkeys(st->ty, &nk);
    for (i = 0; i < nk; i++)
        setns(k[i], name);
    for (i = 0; i < nk; i++) {
        td = htget(st->ty, k[i]);
        if (td->type && td->type->type == Tyname)
            setns(td->type->name, name);
    }
    free(k);
    k = htkeys(st->ns, &nk);
    for (i = 0; i < nk; i++)
        setns(k[i], name);
    free(k);
}
