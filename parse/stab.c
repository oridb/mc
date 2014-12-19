#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
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
    Srcloc loc;
    Node *name;
    Type *type;
};

struct Traitdefn {
    Srcloc loc;
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

int hastype(Stab *st, Node *n)
{
    do {
        if (hthas(st->ty, n))
            return 1;
        st = st->super;
    } while(st);
    return 0;
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

static int mergedecl(Node *old, Node *new)
{
    Node *e, *g;

    if (old->decl.vis == Visexport && new->decl.vis != Visexport) {
        e = old;
        g = new;
    } else if (new->decl.vis == Visexport && old->decl.vis != Visexport) {
        e = new;
        g = old;
    } else {
        return 0;
    }
    old->decl.vis = Visexport;

    if (e->decl.init && g->decl.init)
        fatal(e, "export %s double initialized on line %d", declname(e), g->loc.line);
    if (e->decl.isgeneric != g->decl.isgeneric)
        fatal(e, "export %s declared with different genericness on line %d", declname(e), g->loc.line);
    if (e->decl.isconst != g->decl.isconst)
        fatal(e, "export %s declared with different constness on line %d", declname(e), g->loc.line);
    if (e->decl.isconst != g->decl.isconst)
        fatal(e, "export %s declared with different externness on line %d", declname(e), g->loc.line);

    if (new->decl.name->name.ns)
        setns(old->decl.name, new->decl.name->name.ns);
    if (e->decl.type->type == Tyvar)
        e->decl.type = g->decl.type;
    else if (g->decl.type->type == Tyvar)
        g->decl.type = e->decl.type;

    if (!e->decl.init)
        e->decl.init = g->decl.init;
    else if (!g->decl.init)
        g->decl.init = e->decl.init;

    /* FIXME: check compatible typing */
    old->decl.ishidden = e->decl.ishidden || g->decl.ishidden;
    old->decl.isimport = e->decl.isimport || g->decl.isimport;
    old->decl.isnoret = e->decl.isnoret || g->decl.isnoret;
    old->decl.isexportinit = e->decl.isexportinit || g->decl.isexportinit;
    old->decl.isglobl = e->decl.isglobl || g->decl.isglobl;
    old->decl.ispkglocal = e->decl.ispkglocal || g->decl.ispkglocal;
    return 1;
}

void putdcl(Stab *st, Node *s)
{
    Node *old;

    old = htget(st->dcl, s->decl.name);
    if (!old)
        forcedcl(st, s);
    else if (!mergedecl(old, s))
        fatal(s, "%s already declared on line %d", namestr(s->decl.name), old->loc.line);
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

int mergetype(Type *old, Type *new)
{
    if (old->vis == Visexport && new->vis != Visexport) {
        if (!old->sub && new->sub) {
            old->sub = new->sub;
            old->nsub = new->nsub;
            return 1;
        }
    } else if (new->vis == Visexport && old->vis != Visexport) {
        if (!new->sub && old->sub) {
            new->sub = old->sub;
            new->nsub = old->nsub;
            return 1;
        }
    }
    return 0;
}

void puttype(Stab *st, Node *n, Type *t)
{
    Tydefn *td;
    Type *ty;

    if (st->name)
        setns(n, namestr(st->name));
    if (st->name && t && t->type == Tyname)
        setns(t->name, namestr(st->name));

    ty = gettype(st, n);
    if (!ty) {
        if (hastype(st, n)) {
            t->vis = Visexport;
            updatetype(st, n, t);
        } else {
            td = xalloc(sizeof(Tydefn));
            td->loc = n->loc;
            td->name = n;
            td->type = t;
            htput(st->ty, td->name, td);
        }
    } else if (!mergetype(ty, t)) {
        fatal(n, "Type %s already defined", tystr(gettype(st, n)));
    }
}

void putucon(Stab *st, Ucon *uc)
{
    if (getucon(st, uc->name))
        lfatal(uc->loc, "union constructor %s already defined", namestr(uc->name));
    htput(st->uc, uc->name, uc);
}

void puttrait(Stab *st, Node *n, Trait *c)
{
    Traitdefn *td;

    if (gettrait(st, n))
        fatal(n, "Trait %s already defined", namestr(n));
    if (gettype(st, n))
        fatal(n, "Trait %s already defined as a type", namestr(n));
    td = xalloc(sizeof(Tydefn));
    td->loc = n->loc;
    td->name = n;
    td->trait = c;
    htput(st->tr, td->name, td);
}

void putimpl(Stab *st, Node *n)
{
    if (getimpl(st, n))
        fatal(n, "Trait %s already implemented over %s", namestr(n->impl.traitname), tystr(n->impl.type));
    if (st->name)
        setns(n->impl.traitname, namestr(st->name));
    htput(st->impl, n, n);
}

Node *getimpl(Stab *st, Node *n)
{
    Node *imp;
    
    do {
        if ((imp = htget(st->impl, n)))
            return imp;
        st = st->super;
    } while (st);
    return NULL;
}

void putns(Stab *st, Stab *scope)
{
    Stab *s;

    s = getns(st, scope->name);
    if (s)
        fatal(scope->name, "Ns %s already defined", namestr(s->name));
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
    st->name = mkname(Zloc, name);
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
