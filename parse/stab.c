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

#include "util.h"
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
static size_t stabstkoff;

static Tyenv *envstk[Maxstabdepth];
static size_t envstkoff;


/* name hashing: we want namespaced lookups to find the
 * name even if we haven't set the namespace up, since
 * we can update it after the fact. */
ulong
nsnamehash(void *n)
{
	return strhash(namestr(n));
}

int
nsnameeq(void *a, void *b)
{
	return a == b || !strcmp(namestr(a), namestr(b));
}

static ulong
implhash(void *p)
{
	Node *n;
	ulong h;

	n = p;
	h = nsnamehash(n->impl.traitname);
	h *= tyhash(n->impl.type);
	return h;
}

static int
impleq(void *pa, void *pb)
{
	Node *a, *b;

	a = pa;
	b = pb;
	if (nsnameeq(a->impl.traitname, b->impl.traitname))
		return tyeq(a->impl.type, b->impl.type);
	return 0;
}

Stab *
mkstab(int isfunc)
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

Stab *
curstab()
{
	assert(stabstkoff > 0);
	return stabstk[stabstkoff - 1];
}

void
pushstab(Stab *st)
{
	assert(stabstkoff < Maxstabdepth);
	stabstk[stabstkoff++] = st;
}

void
popstab()
{
	assert(stabstkoff > 0);
	stabstkoff--;
}

ulong
paramhash(void *p)
{
	return strhash(((Type*)p)->pname);
}

int
parameq(void *a, void *b)
{
	return streq(((Type*)a)->pname, ((Type*)b)->pname);
}

Tyenv*
mkenv()
{
	Tyenv *e;

	e = malloc(sizeof(Tyenv));
	e->super = NULL;
	e->tab = mkht(paramhash, parameq);
	return e;
}

Tyenv*
curenv()
{
	if (!envstkoff)
		return NULL;
	else
		return envstk[envstkoff - 1];
}

void
pushenv(Tyenv *env)
{
	if (!env)
		return;
	assert(envstkoff < Maxstabdepth);
	envstk[envstkoff++] = env;
}

void
popenv(Tyenv *env)
{
	if (!env)
		return;
	assert(envstkoff > 0);
	envstkoff--;
}

Stab *
findstab(Stab *st, Node *n)
{
	Stab *ns;

	if (n->name.ns) {
		ns = getns(n->name.ns);
		if (!ns) {
			ns = mkstab(0);
			updatens(ns, n->name.ns);
		}
		st = ns;
	}
	return st;
}

Node *
getclosed(Stab *st, Node *n)
{
	while (st && !st->env)
		st = st->super;
	if (st)
		return htget(st->env, n);
	return NULL;
}

Node **
getclosure(Stab *st, size_t *n)
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

static void
envclose(Stab *start, Stab *found, Stab *fn, Node *s)
{
	Stab *st;

	for (st = start; st != found; st = st->super)
		if (st->env && !s->decl.isglobl && !s->decl.isgeneric)
			htput(st->env, s->decl.name, s);
}

/*
 * Searches for declarations from current
 * scope, and all enclosing scopes. Doe
 * not resolve namespaces -- that is the job
 * of the caller of this function.
 *
 * If a resoved name is not global, and i
 * not in the current scope, it is recorded
 * in the scope's closure.
 */
Node *
getdcl(Stab *start, Node *n)
{
	Stab *st, *fn;
	Node *s;

	fn = NULL;
	st = start;
	do {
		s = htget(st->dcl, n);
		if (s) {
			envclose(start, st, fn, s);
			return s;
		}
		if (!fn && st->env)
			fn = st;
		st = st->super;
	} while (st);
	return NULL;
}

void
putlbl(Stab *st, char *name, Node *lbl)
{
	assert(st && st->isfunc);
	if (hthas(st->lbl, name))
		fatal(lbl, "duplicate label %s, first defined on line %d\n", name, lbl->loc.line);
	htput(st->lbl, name, lbl);
}

Node *
getlbl(Stab *st, Srcloc loc, char *name)
{
	while (st && !st->isfunc)
		st = st->super;
	if (!st || !hthas(st->lbl, name))
		return NULL;
	return htget(st->lbl, name);
}

Type *
gettype_l(Stab *st, Node *n)
{
	Tydefn *t;

	if ((t = htget(st->ty, n)))
		return t->type;
	return NULL;
}

Type *
gettype(Stab *st, Node *n)
{
	Tydefn *t;

	do {
		if ((t = htget(st->ty, n)))
			return t->type;
		st = st->super;
	} while (st);
	return NULL;
}

int
hastype(Stab *st, Node *n)
{
	do {
		if (hthas(st->ty, n))
			return 1;
		st = st->super;
	} while (st);
	return 0;
}

Ucon *
getucon(Stab *st, Node *n)
{
	Ucon *uc;

	do {
		if ((uc = htget(st->uc, n)))
			return uc;
		st = st->super;
	} while (st);
	return NULL;
}

Trait *
gettrait(Stab *st, Node *n)
{
	Traitdefn *c;

	if (n->name.ns)
		st = getns(n->name.ns);
	do {
		if ((c = htget(st->tr, n)))
			return c->trait;
		st = st->super;
	} while (st);
	return NULL;
}

Stab *
getns(char *name) {
	return htget(file.ns, name);
}

static int
mergedecl(Node *old, Node *new)
{
	Node *e, *g;

	if (old->decl.isimport && new->decl.isimport) {
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
	if (old->decl.env || e->decl.env) {
		if (!old->decl.env)
			old->decl.env = e->decl.env;
		if (!e->decl.env)
			e->decl.env = old->decl.env;
		bindtype(e->decl.env, old->decl.type);
		bindtype(old->decl.env, e->decl.type);
	}

	/* FIXME: check compatible typing */
	old->decl.ishidden = e->decl.ishidden || g->decl.ishidden;
	old->decl.isimport = e->decl.isimport || g->decl.isimport;
	old->decl.isnoret = e->decl.isnoret || g->decl.isnoret;
	old->decl.isexportval = e->decl.isexportval || g->decl.isexportval;
	old->decl.isglobl = e->decl.isglobl || g->decl.isglobl;
	old->decl.ispkglocal = e->decl.ispkglocal || g->decl.ispkglocal;
	old->decl.isextern = e->decl.isextern || g->decl.isextern;
	return 1;
}

void
forcedcl(Stab *st, Node *s)
{
	setns(s->decl.name, st->name);
	htput(st->dcl, s->decl.name, s);
	assert(htget(st->dcl, s->decl.name) != NULL);
}

void
putdcl(Stab *st, Node *s)
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

void
updatetype(Stab *st, Node *n, Type *t)
{
	Tydefn *td;

	td = htget(st->ty, n);
	if (!td)
		die("no type %s to update", namestr(n));
	td->type = t;
}

int
mergetype(Type *old, Type *new)
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

void
puttype(Stab *st, Node *n, Type *t)
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
		fatal(n, "type %s already declared on %s:%d", tystr(ty), fname(ty->loc),
				lnum(ty->loc));
	}
}

void
putucon(Stab *st, Ucon *uc)
{
	Ucon *old;

	old = getucon(st, uc->name);
	if (old)
		lfatal(old->loc, "`%s already defined on %s:%d",
			namestr(uc->name), fname(uc->loc), lnum(uc->loc));
	setns(uc->name, st->name);
	htput(st->uc, uc->name, uc);
}

static int
mergetrait(Trait *old, Trait *new)
{
	int hidden;

	hidden = old->ishidden && new->ishidden;
	if (old->isproto && !new->isproto)
		*old = *new;
	else if (new->isproto && !old->isproto)
		*new = *old;
	else if (!new->isimport && !old->isimport)
		if (new->vis == Vishidden || old->vis == Vishidden)
			return 0;
	new->ishidden = hidden;
	old->ishidden = hidden;
	return 1;
}

void
puttrait(Stab *st, Node *n, Trait *c)
{
	Traitdefn *td;
	Trait *t;
	Type *ty;

	st = findstab(st, n);
	t = gettrait(st, n);
	if (t) {
		if (mergetrait(t, c))
			return;
		fatal(n, "trait %s already defined on %s:%d",
			namestr(n), fname(t->loc), lnum(t->loc));
	}
	ty = gettype(st, n);
	if (ty)
		fatal(n, "trait %s defined as a type on %s:%d",
			namestr(n), fname(ty->loc), lnum(ty->loc));
	td = xalloc(sizeof(Traitdefn));
	td->loc = n->loc;
	td->name = n;
	td->trait = c;
	setns(td->name, st->name);
	htput(st->tr, td->name, td);
}

static int
mergeimpl(Node *old, Node *new)
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

void
putimpl(Stab *st, Node *n)
{
	Node *impl;

	st = findstab(st, n->impl.traitname);
	impl = getimpl(st, n);
	if (impl && !mergeimpl(impl, n))
		fatal(n, "trait %s already implemented over %s at %s:%d",
			namestr(n->impl.traitname), tystr(n->impl.type),
			fname(n->loc), lnum(n->loc));
	/* if this is not a duplicate, record it for later export */
	if (!impl)
		lappend(&file.impl, &file.nimpl, n);
	/*
	 The impl is not defined in this file, so setting the
	 trait name would be a bug here.
	 */
	setns(n->impl.traitname, st->name);
	htput(st->impl, n, n);
}

Node *
getimpl(Stab *st, Node *n)
{
	Node *imp;

	do {
		if ((imp = htget(st->impl, n)))
			return imp;
		st = st->super;
	} while (st);
	return NULL;
}

void
putns(Stab *scope)
{
	Stab *s;

	s = getns(scope->name);
	if (s)
		lfatal(Zloc, "Namespace %s already defined", scope->name);
	htput(file.ns, scope->name, scope);
}

/*
 * Sets the namespace of a symbol table, and
 * changes the namespace of all contained symbol
 * to match it.
 */
void
updatens(Stab *st, char *name)
{
	void **k;
	size_t i, nk;
	Traitdefn *tr;
	Tydefn *td;

	if (st->name)
		die("stab %s already has namespace; Can't set to %s", st->name, name);
	st->name = strdup(name);
	htput(file.ns, st->name, st);

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

	k = htkeys(st->tr, &nk);
	for (i = 0; i < nk; i++) {
		tr = htget(st->tr, k[i]);
		setns(tr->name, name);
	}
	free(k);

	k = htkeys(st->uc, &nk);
	for (i = 0; i < nk; i++)
		setns(k[i], name);
	free(k);
}

static void
bindtype_rec(Tyenv *e, Type *t, Bitset *visited)
{
	size_t i;
	Type *tt;

	t = tysearch(t);
	if (bshas(visited, t->tid))
		return;
	bsput(visited, t->tid);
	switch (t->type) {
	case Typaram:
		tt = htget(e->tab, t);
		if (tt && tt != t) {
			tytab[t->tid] = tt;
			for (i = 0; i < t->nspec; i++)
				lappend(&tt->spec, &tt->nspec, t->spec[i]);
		} else if (!boundtype(t)) {
			htput(e->tab, t, t);
		}
		for (i = 0; i < t->nspec; i++) {
			if (t->spec[i]->aux)
				bindtype_rec(e, t->spec[i]->aux, visited);
		}
		break;
	case Tygeneric:
		for (i = 0; i < t->ngparam; i++)
			bindtype_rec(e, t->gparam[i], visited);
		break;
	case Tyname:
		for (i = 0; i < t->narg; i++)
			bindtype_rec(e, t->arg[i], visited);
		break;
	case Tyunres:
		for (i = 0; i < t->narg; i++)
			bindtype_rec(e, t->arg[i], visited);
		break;
	case Tystruct:
		for (i = 0; i < t->nmemb; i++)
			bindtype_rec(e, t->sdecls[i]->decl.type, visited);
		break;
	case Tyunion:
		for (i = 0; i < t->nmemb; i++)
			if (t->udecls[i]->etype)
				bindtype_rec(e, t->udecls[i]->etype, visited);
		break;
	default:
		for (i = 0; i < t->nsub; i++)
			bindtype_rec(e, t->sub[i], visited);
		break;
	}
}

/* Binds the type parameters present in the
 * current type into the type environment */
void
bindtype(Tyenv *env, Type *t)
{
	Bitset *visited;

	if (!t || !env)
		return;
	visited = mkbs();
	bindtype_rec(env, t, visited);
	bsfree(visited);
}

/* If the current environment binds a type,
 * we return true */
Type*
boundtype(Type *t)
{
	Tyenv *e;
	Type *r;

	if (t->type != Typaram)
		return NULL;
	for (e = curenv(); e; e = e->super) {
		r = htget(e->tab, t);
		if (r)
			return r;
	}
	return NULL;
}
