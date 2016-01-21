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
ulong nsnamehash(void *n)
{
	return strhash(namestr(n));
}

int nsnameeq(void *a, void *b)
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

Stab *findstab(Stab *st, Node *n)
{
	Stab *ns;

	if (n->name.ns) {
		ns = getns(file, n->name.ns);
		if (!ns) {
			ns = mkstab(0);
			updatens(ns, n->name.ns);
		}
		st = ns;
	}
	return st;
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

Stab *mkstab(int isfunc)
{
	Stab *st;

	st = zalloc(sizeof(Stab));
	st->dcl = mkht(nsnamehash, nsnameeq);
	st->ty = mkht(nsnamehash, nsnameeq);
	st->tr = mkht(nsnamehash, nsnameeq);
	st->uc = mkht(nsnamehash, nsnameeq);
	if (isfunc) {
		st->env = mkht(nsnamehash, nsnameeq);
		st->lbl = mkht(strhash, streq);
	}
	st->impl = mkht(implhash, impleq);
	st->isfunc = isfunc;
	return st;
}

Node *getclosed(Stab *st, Node *n)
{
	while (st && !st->env)
		st = st->super;
	if (st)
		return htget(st->env, n);
	return NULL;
}

Node **getclosure(Stab *st, size_t *n)
{
	size_t nkeys, i;
	void **keys;
	Node **vals;

	while (st && !st->env)
		st = st->super;

	if (!st) {
		*n = 0;
		return NULL;
	}

	vals = NULL;
	*n = 0;
	keys = htkeys(st->env, &nkeys);
	for (i = 0; i < nkeys; i++)
		lappend(&vals, n, htget(st->env, keys[i]));
	free(keys);
	return vals;
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
	Stab *fn;

	fn = NULL;
	do {
		s = htget(st->dcl, n);
		if (s) {
			/* record that this is in the closure of this scope */
			if (fn && !s->decl.isglobl)
				htput(fn->env, s->decl.name, s);
			return s;
		}
		if (!fn && st->env)
			fn = st;
		st = st->super;
	} while (st);
	return NULL;
}

void putlbl(Stab *st, char *name, Node *lbl)
{
	while (st && !st->isfunc)
		st = st->super;
	if (!st) 
		fatal(lbl, "label %s defined outside function\n", name);
	if (hthas(st->lbl, name))
		fatal(lbl, "duplicate label %s, first defined on line %d\n", name, lbl->loc.line);
	htput(st->lbl, name, lbl);
}

Node *getlbl(Stab *st, Srcloc loc, char *name)
{
	while (st && !st->isfunc)
		st = st->super;
	if (!st || !hthas(st->lbl, name))
		return NULL;
	return htget(st->lbl, name);
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
	} while (st);
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

Stab *getns(Node *file, char *name) {
	return htget(file->file.ns, name);
}

static int mergedecl(Node *old, Node *new)
{
	Node *e, *g;

	if (old->decl.ishidden || new->decl.ishidden) {
		old->decl.ishidden = old->decl.ishidden && new->decl.ishidden;
		return 1;
	}
	if (old->decl.isextern || new->decl.isextern) {
		old->decl.isextern = old->decl.isextern && new->decl.isextern;
		return 1;
	}
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
		fatal(e, "export %s double initialized on %s:%d", declname(e), fname(g->loc),
				lnum(g->loc));
	if (e->decl.isgeneric != g->decl.isgeneric)
		fatal(e, "export %s declared with different genericness on %s:%d", declname(e),
				fname(g->loc), lnum(g->loc));
	if (e->decl.isconst != g->decl.isconst)
		fatal(e, "export %s declared with different constness on %s:%d", declname(e),
				fname(g->loc), lnum(g->loc));
	if (e->decl.isextern != g->decl.isextern)
		fatal(e, "export %s declared with different externness on %s:%d", declname(e),
				fname(g->loc), lnum(g->loc));

	setns(old->decl.name, new->decl.name->name.ns);
	if (e->decl.type->type == Tyvar)
		e->decl.type = g->decl.type;
	else if (g->decl.type->type == Tyvar)
		g->decl.type = e->decl.type;
	else
		fatal(e, "FIXME: types on both prototype and decl not yet allowed\n");

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
	old->decl.isextern = e->decl.isextern || g->decl.isextern;
	return 1;
}

void forcedcl(Stab *st, Node *s)
{
	setns(s->decl.name, st->name);
	htput(st->dcl, s->decl.name, s);
	assert(htget(st->dcl, s->decl.name) != NULL);
}

void putdcl(Stab *st, Node *s)
{
	Node *old;

	st = findstab(st, s->decl.name);
	old = htget(st->dcl, s->decl.name);
	if (!old)
		forcedcl(st, s);
	else if (!mergedecl(old, s))
		fatal(old, "%s already declared on %s:%d", namestr(s->decl.name), fname(s->loc),
				lnum(s->loc));
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
	if (!new) {
		lfatal(old->loc, "double prototyping of %s", tystr(old));
	} else if (old->vis == Visexport && new->vis != Visexport) {
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

	st = findstab(st, n);
	setns(n, st->name);
	if (st->name && t && t->name)
		setns(t->name, st->name);

	ty = gettype(st, n);
	if (!ty) {
		if (t && hastype(st, n)) {
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
		fatal(n, "Type %s already declared on %s:%d", tystr(ty), fname(ty->loc),
				lnum(ty->loc));
	}
}

void putucon(Stab *st, Ucon *uc)
{
	Ucon *old;

	old = getucon(st, uc->name);
	if (old)
		lfatal(old->loc, "`%s already defined on %s:%d", namestr(uc->name), fname(uc->loc),
				lnum(uc->loc));
	htput(st->uc, uc->name, uc);
}

static int mergetrait(Trait *old, Trait *new)
{
	if (old->isproto && !new->isproto)
		*old = *new;
	else if (new->isproto && !old->isproto)
		*new = *old;
	else
		return 0;
	return 1;
}

void puttrait(Stab *st, Node *n, Trait *c)
{
	Traitdefn *td;
	Trait *t;
	Type *ty;

	st = findstab(st, n);
	t = gettrait(st, n);
	if (t && !mergetrait(t, c))
		fatal(n, "Trait %s already defined on %s:%d", namestr(n), fname(t->loc),
				lnum(t->loc));
	ty = gettype(st, n);
	if (ty)
		fatal(n, "Trait %s defined as a type on %s:%d", namestr(n), fname(ty->loc),
				lnum(ty->loc));
	td = xalloc(sizeof(Traitdefn));
	td->loc = n->loc;
	td->name = n;
	td->trait = c;
	setns(td->name, st->name);
	htput(st->tr, td->name, td);
}

static int mergeimpl(Node *old, Node *new)
{
	Vis vis;

	/* extern traits should be deduped in use.c. */
	if (old->impl.isextern && new->impl.isextern)
		return 1;
	if (old->impl.vis > new->impl.vis)
		vis = old->impl.vis;
	else
		vis = new->impl.vis;
	old->impl.vis = vis;
	new->impl.vis = vis;
	if (old->impl.isproto && !new->impl.isproto)
		*old = *new;
	else if (new->impl.isproto && !old->impl.isproto)
		*new = *old;
	else
		return 0;
	return 1;
}

void putimpl(Stab *st, Node *n)
{
	Node *impl;

	st = findstab(st, n->impl.traitname);
	impl = getimpl(st, n);
	if (impl && !mergeimpl(impl, n))
		fatal(n, "Trait %s already implemented over %s at %s:%d",
				namestr(n->impl.traitname), tystr(n->impl.type), fname(n->loc), lnum(n->loc));
	/*
	 * The impl is not defined in this file, so setting the trait name would be a bug here.
	*/
	setns(n->impl.traitname, st->name);
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

void putns(Node *file, Stab *scope)
{
	Stab *s;

	s = getns(file, scope->name);
	if (s)
		lfatal(Zloc, "Namespace %s already defined", scope->name);
	htput(file->file.ns, scope->name, scope);
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
		die("Stab %s already has namespace; Can't set to %s", st->name, name);
	st->name = strdup(name);
	htput(file->file.ns, st->name, st);

	k = htkeys(st->dcl, &nk);
	for (i = 0; i < nk; i++)
		setns(k[i], name);
	free(k);

	k = htkeys(st->ty, &nk);
	for (i = 0; i < nk; i++)
		setns(k[i], name);
	for (i = 0; i < nk; i++) {
		td = htget(st->ty, k[i]);
		if (td->type && (td->type->type == Tyname || td->type->type == Tygeneric))
			setns(td->type->name, name);
	}
	free(k);
}
