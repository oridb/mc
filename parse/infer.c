#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "util.h"
#include "parse.h"

typedef struct Traitmap Traitmap;
typedef struct Postcheck Postcheck;

struct Traitmap {
	Bitset *traits;
	Traitmap *sub[Ntypes];

	Htab *name;	/* name => bitset(traits) */
	Type **filter;
	size_t nfilter;
	Trait **filtertr;
	size_t nfiltertr;
};

struct Postcheck {
	Node *node;
	Stab *stab;
	Tyenv *env;
};


int allowhidden;

static void infernode(Node **np, Type *ret, int *sawret);
static void inferexpr(Node **np, Type *ret, int *sawret);
static void inferdecl(Node *n);
static int tryconstrain(Type *ty, Trait *tr, int update);

static Type *tyfreshen(Tysubst *subst, Type *orig);
static Type *tf(Type *t);
static Type *basetype(Type *a);

static Type *unify(Node *ctx, Type *a, Type *b);
static Type *tyfix(Node *ctx, Type *orig, int noerr);
static void typesub(Node *n, int noerr);

/* tracking where we are in the inference */
static int ingeneric;
static int inaggr;
static int indentdepth;
static Srcloc *unifysrc;
static size_t nunifysrc;

/* post-inference checking/unification */
static Htab *delayed;
static Postcheck **postcheck;
static size_t npostcheck;

/* generic declarations to be specialized */
static Node **genericdecls;
static size_t ngenericdecls;
static Node **impldecl;
static size_t nimpldecl;

/* specializations of generics */
static Node **specializations;
static size_t nspecializations;
static Stab **specializationscope;
static size_t nspecializationscope;
static Traitmap *traitmap;
static Bitset *tytraits[Ntypes];


static void
ctxstrcall(char *buf, size_t sz, Node *n)
{
	char *p, *end, *sep, *t;
	size_t nargs, i;
	Node **args;
	Type *et;

	args = n->expr.args;
	nargs = n->expr.nargs;
	p = buf;
	end = buf + sz;
	sep = "";

	if (exprop(args[0]) == Ovar)
		p += bprintf(p, end - p, "%s(", namestr(args[0]->expr.args[0]));
	else
		p += bprintf(p, end - p, "<e>(");
	for (i = 1; i < nargs; i++) {
		et = tyfix(NULL, exprtype(args[i]), 1);
		if (et != NULL)
			t = tystr(et);
		else
			t = strdup("?");

		if (exprop(args[i]) == Ovar)
			p += bprintf(p, end - p, "%s%s:%s", sep, namestr(args[i]->expr.args[0]), t);
		else
			p += bprintf(p, end - p, "%s<e%zd>:%s", sep, i, t);
		sep = ", ";
		free(t);
	}
	if (exprtype(args[0])->nsub)
		t = tystr(tyfix(NULL, exprtype(args[0])->sub[0], 1));
	else
		t = strdup("unknown");
	p += bprintf(p, end - p, "): %s", t);
	free(t);
}

static char *
nodetystr(Node *n)
{
	Type *t;

	t = NULL;
	if (n->type == Nexpr && exprtype(n) != NULL)
		t = tysearch(exprtype(n));
	else if (n->type == Ndecl && decltype(n) != NULL)
		t = tysearch(decltype(n));

	if (t && tybase(t)->type != Tyvar)
		return tystr(t);
	else
		return strdup("unknown");
}

static void
marksrc(Type *t, Srcloc l)
{
	t = tf(t);
	if (t->tid >= nunifysrc) {
		unifysrc = zrealloc(unifysrc, nunifysrc*sizeof(Srcloc), (t->tid + 1)*sizeof(Srcloc));
		nunifysrc = t->tid + 1;
	}
	if (unifysrc[t->tid].line <= 0)
		unifysrc[t->tid] = l;
}

static char *
srcstr(Type *ty)
{
	char src[128];
	Srcloc l;
	char *s;

	src[0] = 0;
	if (nunifysrc > ty->tid && unifysrc[ty->tid].line > 0) {
		l = unifysrc[ty->tid];
		s = tystr(ty);
		snprintf(src, sizeof src, "\n\t%s from %s:%d", s, fname(l), lnum(l));
		free(s);
	}
	return strdup(src);
}

/* Tries to give a good string describing the context
 * for the sake of error messages. */
static char *
ctxstr(Node *n)
{
	char *t, *t1, *t2, *t3;
	char *s, *d;
	size_t nargs;
	Node **args;
	char buf[512];

	if (!n)
		return strdup("???");
	switch (n->type) {
	default:
		s = strdup(nodestr[n->type]);
		break;
	case Ndecl:
		 d = declname(n);
		 t = nodetystr(n);
		 bprintf(buf, sizeof buf, "%s:%s", d, t);
		 s = strdup(buf);
		 free(t);
		 break;
	case Nname:	s = strdup(namestr(n));	break;
	case Nexpr:
		args = n->expr.args;
		nargs = n->expr.nargs;
		t1 = NULL;
		t2 = NULL;
		t3 = NULL;
		if (exprop(n) == Ovar)
			d = namestr(args[0]);
		else
			d = opstr[exprop(n)];
		t = nodetystr(n);
		if (nargs >= 1)
			t1 = nodetystr(args[0]);
		if (nargs >= 2)
			t2 = nodetystr(args[1]);
		if (nargs >= 3)
			t3 = nodetystr(args[2]);

		switch (opclass[exprop(n)]) {
		case OTpre:	bprintf(buf, sizeof buf, "%s<e:%s>", oppretty[exprop(n)], t1);	break;
		case OTpost:	bprintf(buf, sizeof buf, "<e:%s>%s", t1, oppretty[exprop(n)]);	break;
		case OTzarg:	bprintf(buf, sizeof buf, "%s", oppretty[exprop(n)]);	break;
		case OTbin:
			bprintf(buf, sizeof buf, "<e1:%s> %s <e2:%s>", t1, oppretty[exprop(n)], t2);
			break;
		case OTmisc:
			switch (exprop(n)) {
			case Ovar:	bprintf(buf, sizeof buf, "%s:%s", namestr(args[0]), t);	break;
			case Ocall:	ctxstrcall(buf, sizeof buf,n);	break;
			case Oidx:
				if (exprop(args[0]) == Ovar)
					bprintf(buf, sizeof buf, "%s[<e1:%s>]", namestr(args[0]->expr.args[0]), t2);
				else
					bprintf(buf, sizeof buf, "<sl:%s>[<e1%s>]", t1, t2);
				break;
			case Oslice:
				if (exprop(args[0]) == Ovar)
					bprintf(buf, sizeof buf, "%s[<e1:%s>:<e2:%s>]", namestr(args[0]->expr.args[0]), t2, t3);
				else
					bprintf( buf, sizeof buf, "<sl:%s>[<e1%s>:<e2:%s>]", t1, t2, t3);
				break;
			case Omemb:
				bprintf(buf, sizeof buf, "<%s>.%s", t1, namestr(args[1]));
				break;
			case Otupmemb:
				bprintf(buf, sizeof buf, "<%s>.%llu", t1, args[1]->lit.intval);
				break;
			default:
				bprintf(buf, sizeof buf, "%s:%s", d, t);
				break;
			}
			break;
		default: bprintf(buf, sizeof buf, "%s", d); break;
		}
		free(t);
		free(t1);
		free(t2);
		free(t3);
		s = strdup(buf);
		break;
	}
	return s;
}

static void
addspecialization(Node *n, Stab *stab)
{
	Node *dcl;

	dcl = decls[n->expr.did];
	lappend(&specializationscope, &nspecializationscope, stab);
	lappend(&specializations, &nspecializations, n);
	lappend(&genericdecls, &ngenericdecls, dcl);
}

static void
adddispspecialization(Node *n, Stab *stab)
{
	Trait *tr;
	Type *ty;

	tr = traittab[Tcdisp];
	ty = exprtype(n);
	assert(tr->nproto == 1);
	if (hthas(tr->proto[0]->decl.impls, ty))
		return;
	lappend(&specializationscope, &nspecializationscope, stab);
	lappend(&specializations, &nspecializations, n);
	lappend(&genericdecls, &ngenericdecls, tr->proto[0]);
}

static void
additerspecialization(Node *n, Stab *stab)
{
	Trait *tr;
	Type *ty;
	size_t i;

	tr = traittab[Tciter];
	ty = exprtype(n->iterstmt.seq);
	if (ty->type == Tyslice || ty->type == Tyarray || ty->type == Typtr)
		return;
	ty = tyfreshen(NULL, ty);
	for (i = 0; i < tr->nproto; i++) {
		ty = exprtype(n->iterstmt.seq);
		if (hthas(tr->proto[i]->decl.impls, ty))
			continue;
		lappend(&specializationscope, &nspecializationscope, stab);
		lappend(&specializations, &nspecializations, n);
		lappend(&genericdecls, &ngenericdecls, tr->proto[i]);
	}
}

static void
delayedcheck(Node *n)
{
	Postcheck *pc;

	pc = xalloc(sizeof(Postcheck));
	pc->node = n;
	pc->stab = curstab();
	pc->env = curenv();
	lappend(&postcheck, &npostcheck, pc);
}

static void
typeerror(Type *a, Type *b, Node *ctx, char *msg)
{
	char *t1, *t2, *s1, *s2, *c;

	t1 = tystr(tyfix(NULL, a, 1));
	t2 = tystr(tyfix(NULL, b, 1));
	s1 = srcstr(a);
	s2 = srcstr(b);
	c = ctxstr(ctx);
	if (msg)
		fatal(ctx, "type \"%s\" incompatible with \"%s\" near %s: %s%s%s", t1, t2, c, msg, s1, s2);
	else
		fatal(ctx, "type \"%s\" incompatible with \"%s\" near %s%s%s", t1, t2, c, s1, s2);
	free(t1);
	free(t2);
	free(s1);
	free(s2);
	free(c);
}

/* Set a scope's enclosing scope up correctly. */
static void
setsuperenv(Tyenv *e, Tyenv *super)
{
	if (e)
		e->super = super;
}

/* Set a scope's enclosing scope up correctly. */
static void
setsuper(Stab *st, Stab *super)
{
	Stab *s;

	/* verify that we don't accidentally create loops */
	for (s = super; s; s = s->super)
		assert(s->super != st);
	st->super = super;
}


/* Checks if a type that directly contains itself.
 * Recursive types that contain themselves through
 * pointers or slices are fine, but any other self-inclusion
 * would lead to a value of infinite size */
static int
occurs_rec(Type *sub, Bitset *bs)
{
	size_t i;

	sub = tf(sub);
	if (bshas(bs, sub->tid))
		return 1;
	bsput(bs, sub->tid);
	switch (sub->type) {
	case Typtr:
	case Tyslice:
		break;
	case Tystruct:
		for (i = 0; i < sub->nmemb; i++)
			if (occurs_rec(decltype(sub->sdecls[i]), bs))
				return 1;
		break;
	case Tyunion:
		for (i = 0; i < sub->nmemb; i++) {
			if (!sub->udecls[i]->etype)
				continue;
			if (occurs_rec(sub->udecls[i]->etype, bs))
				return 1;
		}
		break;
	default:
		for (i = 0; i < sub->nsub; i++)
			if (occurs_rec(sub->sub[i], bs))
				return 1;
		break;
	}
	bsdel(bs, sub->tid);
	return 0;
}

static int
occursin(Type *a, Type *b)
{
	Bitset *bs;
	int r;

	bs = mkbs();
	a = tf(a);
	bsput(bs, a->tid);
	r = occurs_rec(b, bs);
	bsfree(bs);
	return r;
}

static int
occurs(Type *t)
{
	Bitset *bs;
	int r;

	bs = mkbs();
	r = occurs_rec(t, bs);
	bsfree(bs);
	return r;
}

static int
needfreshenrec(Type *t, Bitset *visited)
{
	size_t i;

	t = tysearch(t);
	if (bshas(visited, t->tid))
		return 0;
	bsput(visited, t->tid);
	switch (t->type) {
	case Typaram: return 1;
	case Tygeneric: return 1;
	case Tyname:
		for (i = 0; i < t->narg; i++)
			if (needfreshenrec(t->arg[i], visited))
				return 1;
		return needfreshenrec(t->sub[0], visited);
	case Tystruct:
		for (i = 0; i < t->nmemb; i++)
			if (needfreshenrec(decltype(t->sdecls[i]), visited))
				return 1;
		break;
	case Tyunion:
		for (i = 0; i < t->nmemb; i++)
			if (t->udecls[i]->etype && needfreshenrec(t->udecls[i]->etype, visited))
				return 1;
		break;
	default:
		for (i = 0; i < t->nsub; i++)
			if (needfreshenrec(t->sub[i], visited))
				return 1;
		break;
	}
	return 0;
}

static int
needfreshen(Type *t)
{
	Bitset *visited;
	int ret;

	visited = mkbs();
	ret = needfreshenrec(t, visited);
	bsfree(visited);
	return ret;
}

/* Freshens the type of a declaration. */
static Type *
tyfreshen(Tysubst *subst, Type *orig)
{
	Type *ty, *base;

	if (!needfreshen(orig))
		return orig;
	pushenv(orig->env);
	if (!subst) {
		subst = mksubst();
		ty = tyspecialize(orig, subst, delayed);
		substfree(subst);
	} else {
		ty = tyspecialize(orig, subst, delayed);
	}
	ty->spec = orig->spec;
	ty->nspec = orig->nspec;
	base = basetype(ty);
	if (base)
		ty->seqaux = base;
	popenv(orig->env);
	return ty;
}

/* Resolves a type and all its subtypes recursively. */
static void
tyresolve(Type *t)
{
	size_t i, j;
	Trait *tr;

	if (t->resolved)
		return;
	/* type resolution should never throw errors about non-generic
	 * showing up within a generic type, so we push and pop a generic
	 * around resolution */
	ingeneric++;
	t->resolved = 1;
	/* Walk through aggregate type members */
	pushenv(t->env);
	switch (t->type) {
	case Tystruct:
		inaggr++;
		for (i = 0; i < t->nmemb; i++)
			infernode(&t->sdecls[i], NULL, NULL);
		inaggr--;
		break;
	case Tyunion:
		inaggr++;
		for (i = 0; i < t->nmemb; i++) {
			if (!t->udecls[i]->utype)
				t->udecls[i]->utype = t;
			t->udecls[i]->utype = tf(t->udecls[i]->utype);
			if (t->udecls[i]->etype) {
				tyresolve(t->udecls[i]->etype);
				t->udecls[i]->etype = tf(t->udecls[i]->etype);
			}
		}
		inaggr--;
		break;
	case Tyarray:
		if (!inaggr && !t->asize)
			lfatal(t->loc, "unsized array type outside of struct");
		infernode(&t->asize, NULL, NULL);
		break;
	case Typaram:
		if (!boundtype(t))
			lfatal(t->loc, "type parameter %s is undefined in generic context", tystr(t));
		break;
	default:
		break;
	}

	for (i = 0; i < t->nspec; i++) {
		for (j = 0; j < t->spec[i]->ntrait; j++) {
			tr = gettrait(curstab(), t->spec[i]->trait[j]);
			if (!tr)
				lfatal(t->loc, "trait %s does not exist", ctxstr(t->spec[i]->trait[j]));
			if (!t->trneed)
				t->trneed = mkbs();
			bsput(t->trneed, tr->uid);
			if (nameeq(t->spec[i]->trait[j], traittab[Tciter]->name))
				t->seqaux = t->spec[i]->aux;
			t->spec[i]->trait[j] = tr->name;
		}
	}

	for (i = 0; i < t->nsub; i++) {
		t->sub[i] = tf(t->sub[i]);
		if (t->sub[i] == t) {
			lfatal(t->loc,
			     "%s occurs within %s, leading to infinite type\n",
			     tystr(t->sub[i]), tystr(t));
		}
	}
	if (occurs(t))
		lfatal(t->loc, "type %s includes itself", tystr(t));
	popenv(t->env);
	ingeneric--;
}

Type *
tysearch(Type *t)
{
	if (!t)
		return t;
	while (tytab[t->tid])
		t = tytab[t->tid];
	return t;
}

/* Look up the best type to date in the unification table, returning it */
static Type *
tylookup(Type *t)
{
	Type *lu;
	Stab *ns;

	assert(t != NULL);
	lu = NULL;
	while (1) {
		if (!tytab[t->tid] && t->type == Tyunres) {
			ns = curstab();
			if (t->name->name.ns)
				ns = getns(t->name->name.ns);
			if (!ns)
				fatal(t->name, "no namespace \"%s\"", t->name->name.ns);
			lu = gettype(ns, t->name);
			if (!lu)
				fatal(t->name, "no type %s", tystr(t));
			if (lu && lu != t)
				tytab[t->tid] = lu;
		}

		if (!tytab[t->tid])
			break;
		/* compress paths: shift the link up one level */
		if (tytab[tytab[t->tid]->tid])
			tytab[t->tid] = tytab[tytab[t->tid]->tid];
		t = tytab[t->tid];
	}
	return t;
}

static Type *
tysubstmap(Tysubst *subst, Type *t, Type *orig)
{
	size_t i;

	for (i = 0; i < t->ngparam; i++)
		substput(subst, t->gparam[i], tf(orig->arg[i]));
	t = tyfreshen(subst, t);
	return t;
}

static Type *
tysubst(Type *t, Type *orig)
{
	Tysubst *subst;

	subst = mksubst();
	t = tysubstmap(subst, t, orig);
	substfree(subst);
	return t;
}


/* find the most accurate type mapping we have (ie,
 * the end of the unification chain */
static Type *
tf(Type *orig)
{
	int isgeneric;
	Type *t, *tt;

	assert(orig != NULL);
	t = tylookup(orig);
	isgeneric = t->type == Tygeneric;
	ingeneric += isgeneric;
	pushenv(orig->env);
	tyresolve(t);
	if ((tt = boundtype(t)) != NULL) {
		t = tt;
		tyresolve(t);
	}
	popenv(orig->env);
	/* If this is an instantiation of a generic type, we want the params to
	 * match the instantiation */
	if (orig->type == Tyunres && t->type == Tygeneric) {
		if (t->ngparam != orig->narg) {
			lfatal(orig->loc, "%s incompatibly specialized with %s, declared on %s:%d",
					tystr(orig), tystr(t), file.files[t->loc.file], t->loc.line);
		}
		pushenv(orig->env);
		t = tysubst(t, orig);
		popenv(orig->env);
	}
	ingeneric -= isgeneric;
	return t;
}

/* set the type of any typable node */
static void
settype(Node *n, Type *t)
{
	t = tf(t);
	switch (n->type) {
	case Nexpr:	n->expr.type = t;	break;
	case Ndecl:	n->decl.type = t;	break;
	case Nlit:	n->lit.type = t;	break;
	case Nfunc:	n->func.type = t;	break;
	default: die("untypable node %s", nodestr[n->type]); break;
	}
	if (t->type != Tyvar)
		marksrc(t, n->loc);
}

static Type*
mktylike(Srcloc l, Ty other)
{
	Type *t;

	t = mktyvar(l);
	/* not perfect in general, but good enough for all places mktylike is used. */
	t->trneed = bsdup(tytraits[other]);
	return t;
}

/* Gets the type of a literal value */
static Type *
littype(Node *n)
{
	Type *t;

	t = NULL;
	if (!n->lit.type) {
		switch (n->lit.littype) {
		case Lvoid:	t = mktype(n->loc, Tyvoid);	break;
		case Lchr:	t = mktype(n->loc, Tychar);	break;
		case Lbool:	t = mktype(n->loc, Tybool);	break;
		case Lint:	t = mktylike(n->loc, Tyint);	break;
		case Lflt:	t = mktylike(n->loc, Tyflt64);	break;
		case Lstr:	t = mktyslice(n->loc, mktype(n->loc, Tybyte));	break;
		case Llbl:	t = mktyptr(n->loc, mktype(n->loc, Tyvoid));	break;
		case Lfunc:	t = n->lit.fnval->func.type;	break;
		}
		n->lit.type = t;
	}
	return n->lit.type;
}

static Type *
delayeducon(Type *fallback)
{
	Type *t;

	if (fallback->type != Tyunion)
		return fallback;
	t = mktylike(fallback->loc, fallback->type);
	htput(delayed, t, fallback);
	return t;
}

/* Finds the type of any typable node */
static Type *
type(Node *n)
{
	Type *t;

	switch (n->type) {
	case Nlit:	t = littype(n);	break;
	case Nexpr:	t = n->expr.type;	break;
	case Ndecl:	t = decltype(n);	break;
	case Nfunc:	t = n->func.type;	break;
	default:
		t = NULL;
		die("untypeable node %s", nodestr[n->type]);
		break;
	};
	return tf(t);
}

static Ucon *
uconresolve(Node *n)
{
	Ucon *uc;
	Node **args;
	Stab *ns;

	args = n->expr.args;
	ns = curstab();
	if (args[0]->name.ns)
		ns = getns(args[0]->name.ns);
	if (!ns)
		fatal(n, "no namespace %s\n", args[0]->name.ns);
	uc = getucon(ns, args[0]);
	if (!uc)
		fatal(n, "no union constructor `%s", ctxstr(args[0]));
	if (!uc->etype && n->expr.nargs > 1)
		fatal(n, "nullary union constructor `%s passed arg ", ctxstr(args[0]));
	else if (uc->etype && n->expr.nargs != 2)
		fatal(n, "union constructor `%s needs arg ", ctxstr(args[0]));
	return uc;
}

/* this doesn't walk through named types, so it can't recurse infinitely. */
int
tymatchrank(Type *pat, Type *to)
{
	int match, q;
	size_t i;
	Ucon *puc, *tuc;

	pat = tysearch(pat);
	to = tysearch(to);
	if (pat->type == Typaram) {
		if (!pat->trneed)
			return 0;
		for (i = 0; bsiter(pat->trneed, &i); i++)
			if (!tryconstrain(to, traittab[i], 0))
				return -1;
		return 0;
	} else if (pat->type == Tyvar) {
		return 1;
	} else if (pat->type != to->type) {
		return -1;
	}

	match = 0;
	switch (pat->type) {
	case Tystruct:
		if (pat->nmemb != to->nmemb)
			return -1;
		for (i = 0; i < pat->nmemb; i++) {
			if (!streq(declname(pat->sdecls[i]),declname( to->sdecls[i])))
				return -1;
			q = tymatchrank(decltype(pat->sdecls[i]), decltype(to->sdecls[i]));
			if (q < 0)
				return -1;
			match += q;
		}
		break;
	case Tyunion:
		if (pat->nmemb != to->nmemb)
			return -1;
		for (i = 0; i < pat->nmemb; i++) {
			if (!nameeq(pat->udecls[i], to->udecls[i]))
				return -1;
			puc = pat->udecls[i];
			tuc = to->udecls[i];
			if (puc->etype && tuc->etype) {
				q = tymatchrank(puc->etype, tuc->etype);
				if (q < 0)
					return -1;
				match += q;
			} else if (puc->etype != tuc->etype) {
				return -1;
			}
		}
		break;
	case Tyname:
		if (!nameeq(pat->name, to->name))
			return -1;
		if (pat->narg != to->narg)
			return -1;
		for (i = 0; i < pat->narg; i++) {
			q = tymatchrank(pat->arg[i], to->arg[i]);
			if (q < 0)
				return -1;
			match += q;
		}
		break;
	case Tyarray:
		/* unsized arrays are ok */
		if (pat->asize && to->asize) {
			if (!!litvaleq(pat->asize->expr.args[0], to->asize->expr.args[0]))
				return -1;
		} else if (pat->asize != to->asize) {
			return -1;
		}
		else return tymatchrank(pat->sub[0], to->sub[0]);
		break;
	default:
		if (pat->nsub != to->nsub)
			break;
		if (pat->type == to->type)
			match = 1;
		for (i = 0; i < pat->nsub; i++) {
			q = tymatchrank(pat->sub[i], to->sub[i]);
			if (q < 0)
				return -1;
			match += q;
		}
		break;
	}
	return match;
}

static int
tryconstrain(Type *base, Trait *tr, int update)
{
	Traitmap *tm;
	Bitset *bs;
	Type *ty;
	size_t i;

	while(1) {
		ty = base;
		tm = traitmap->sub[ty->type];
		while (1) {
			if (ty->type == Typaram)
				if (ty->trneed && bshas(ty->trneed, tr->uid))
					return 1;
			if (ty->type == Tyvar) {
				if (!ty->trneed)
					ty->trneed = mkbs();
				if (update)
					bsput(ty->trneed, tr->uid);
				return 1;
			}
			if (bshas(tm->traits, tr->uid))
				return 1;
			if (tm->name && ty->type == Tyname) {
				bs = htget(tm->name, ty->name);
				if (bs && bshas(bs, tr->uid))
					return 1;
			}
			for (i = 0; i < tm->nfilter; i++) {
				if (tm->filtertr[i]->uid != tr->uid)
					continue;
				if (tymatchrank(tm->filter[i], ty) >= 0)
					return 1;
			}
			if (!ty->sub || ty->nsub != 1)
				break;
			ty = ty->sub[0];
			tm = tm->sub[ty->type];
			if (!tm)
				break;
		}
		if (base->type != Tyname)
			break;
		base = base->sub[0];
	}
	return 0;
}

/* Constrains a type to implement the required constraints. On
 * type variables, the constraint is added to the required
 * constraint list. Otherwise, the type is checked to see
 * if it has the required constraint */
static void
constrain(Node *ctx, Type *base, Trait *tr)
{
	if (!tryconstrain(base, tr, 1))
		fatal(ctx, "%s needs trait %s near %s", tystr(base), namestr(tr->name), ctxstr(ctx));
}

static void
traitsfor(Type *base, Bitset *dst)
{
	Traitmap *tm;
	Bitset *bs;
	Type *ty;
	size_t i;

	while(1) {
		ty = base;
		tm = traitmap->sub[ty->type];
		while (1) {
			if (ty->type == Tyvar)
				break;
			if (ty->type == Tyname && ty->ngparam == 0)
				bs = htget(tm->name, ty->name);
			else
				bs = tm->traits;
			if (bs)
				bsunion(dst, bs);
			for (i = 0; i < tm->nfilter; i++) {
				if (tymatchrank(tm->filter[i], ty) >= 0)
					bsput(dst, tm->filtertr[i]->uid);
			}
			if (!tm->sub[ty->type] || ty->nsub != 1)
				break;
			tm = tm->sub[ty->type];
			ty = ty->sub[0];
		}
		if (base->type != Tyname)
			break;
		base = base->sub[0];
	}
}

static void
verifytraits(Node *ctx, Type *a, Type *b)
{
	char traitbuf[64], abuf[64], bbuf[64];
	char asrc[128], bsrc[128];
	Bitset *abs, *bbs;
	size_t i, n;
	Srcloc l;
	char *sep;

	abs = a->trneed;
	if (!abs) {
		abs = mkbs();
		traitsfor(a, abs);
	}
	bbs = b->trneed;
	if (!bbs) {
		bbs = mkbs();
		traitsfor(b, bbs);
	}
	if (!bsissubset(abs, bbs)) {
		sep = "";
		n = 0;
		*traitbuf = 0;
		for (i = 0; bsiter(abs, &i); i++) {
			if (!bshas(bbs, i)) {
				n += bprintf(traitbuf + n, sizeof(traitbuf) - n, "%s%s", sep,
					     namestr(traittab[i]->name));
				sep = ",";
			}
		}
		tyfmt(abuf, sizeof abuf, a);
		tyfmt(bbuf, sizeof bbuf, b);
		bsrc[0] = 0;
		if (nunifysrc > b->tid && unifysrc[b->tid].line > 0) {
			l = unifysrc[b->tid];
			snprintf(bsrc, sizeof asrc, "\n\t%s from %s:%d", bbuf, fname(l), lnum(l));
		}
		fatal(ctx, "%s needs trait %s near %s%s%s",
			bbuf, traitbuf, ctxstr(ctx), srcstr(a), srcstr(b));
	}
	if (!a->trneed)
		bsfree(abs);
	if (!b->trneed)
		bsfree(bbs);
}

/* Merges the constraints on types */
static void
mergetraits(Node *ctx, Type *a, Type *b)
{
	if (b->type == Tyvar) {
		/* make sure that if a = b, both have same traits */
		if (a->trneed && b->trneed)
			bsunion(b->trneed, a->trneed);
		else if (a->trneed)
			b->trneed = bsdup(a->trneed);
		else if (b->trneed)
			a->trneed = bsdup(b->trneed);
	} else {
		verifytraits(ctx, a, b);
	}
}

/* Computes the 'rank' of the type; ie, in which
 * direction should we unify. A lower ranked type
 * should be mapped to the higher ranked (ie, more
 * specific) type. */
static int
tyrank(Type *t)
{
	if (t->type == Tyvar) {
		/* has associated iterator type */
		if (t->seqaux)
			return 1;
		else
			return 0;
	}
	return 2;
}

static void
unionunify(Node *ctx, Type *u, Type *v)
{
	size_t i, j;
	int found;

	if (u->nmemb != v->nmemb)
		fatal(ctx, "can't unify %s and %s near %s%s%s\n",
			tystr(u), tystr(v), ctxstr(ctx),
			srcstr(u), srcstr(v));

	for (i = 0; i < u->nmemb; i++) {
		found = 0;
		for (j = 0; j < v->nmemb; j++) {
			if (!nameeq(u->udecls[i]->name, v->udecls[j]->name))
				continue;
			found = 1;
			if (u->udecls[i]->etype == NULL && v->udecls[j]->etype == NULL)
				continue;
			else if (u->udecls[i]->etype && v->udecls[j]->etype)
				unify(ctx, u->udecls[i]->etype, v->udecls[j]->etype);
			else
				fatal(ctx, "can't unify %s and %s near %s%s%s",
					tystr(u), tystr(v), ctxstr(ctx),
					srcstr(u), srcstr(v));
		}
		if (!found)
			fatal(ctx, "can't unify %s and %s near %s%s%s",
				tystr(u), tystr(v), ctxstr(ctx),
				srcstr(u), srcstr(v));
	}
}

static void
structunify(Node *ctx, Type *u, Type *v)
{
	size_t i, j;
	int found;
	char *ud, *vd;

	if (u->nmemb != v->nmemb)
		fatal(ctx, "can't unify %s and %s near %s%s%s",
			tystr(u), tystr(v), ctxstr(ctx),
			srcstr(u), srcstr(v));

	for (i = 0; i < u->nmemb; i++) {
		found = 0;
		for (j = 0; j < v->nmemb; j++) {
			ud = namestr(u->sdecls[i]->decl.name);
			vd = namestr(v->sdecls[j]->decl.name);
			if (strcmp(ud, vd) == 0) {
				found = 1;
				unify(ctx, type(u->sdecls[i]), type(v->sdecls[j]));
			}
		}
		/* we had at least one missing member */
		if (!found)
			fatal(ctx, "can't unify %s and %s near %s%s%s",
				tystr(u), tystr(v), ctxstr(ctx),
				srcstr(u), srcstr(v));
	}
}

static void
membunify(Node *ctx, Type *u, Type *v)
{
	if (hthas(delayed, u))
		u = htget(delayed, u);
	u = tybase(u);
	if (hthas(delayed, v))
		v = htget(delayed, v);
	v = tybase(v);
	if (u->type == Tyunion && v->type == Tyunion && u != v)
		unionunify(ctx, u, v);
	else if (u->type == Tystruct && v->type == Tystruct && u != v)
		structunify(ctx, u, v);
}

static Type *
basetype(Type *a)
{
	Type *t;

	t = a->seqaux;
	while (!t && a->type == Tyname) {
		a = a->sub[0];
		t = a->seqaux;
	}
	if (!t && (a->type == Tyslice || a->type == Tyarray || a->type == Typtr))
		t = a->sub[0];
	if (t)
		t = tf(t);
	return t;
}

static void
checksize(Node *ctx, Type *a, Type *b)
{
	if (a->asize)
		a->asize = fold(a->asize, 1);
	if (b->asize)
		b->asize = fold(b->asize, 1);
	if (a->asize && exprop(a->asize) != Olit)
		lfatal(ctx->loc, "%s: array size is not constant near %s",
				tystr(a), ctxstr(ctx));
	if (b->asize && exprop(b->asize) != Olit)
		lfatal(ctx->loc, "%s: array size is not constant near %s",
				tystr(b), ctxstr(ctx));
	if (!a->asize)
		a->asize = b->asize;
	else if (!b->asize)
		b->asize = a->asize;
	else if (a->asize && b->asize)
		if (!litvaleq(a->asize->expr.args[0], b->asize->expr.args[0]))
			lfatal(ctx->loc, "array size of %s does not match %s near %s",
				tystr(a), tystr(b), ctxstr(ctx));
}

static int
hasargs(Type *t)
{
	return t->type == Tyname && t->narg > 0;
}

/* Unifies two types, or errors if the types are not unifiable. */
static Type *
unify(Node *ctx, Type *u, Type *v)
{
	Type *t, *r;
	Type *a, *b;
	Type *ea, *eb;
	size_t i;

	/* a ==> b */
	a = tf(u);
	b = tf(v);
	if (a->tid == b->tid)
		return a;

	/* we unify from lower to higher ranked types */
	if (tyrank(b) < tyrank(a)) {
		t = a;
		a = b;
		b = t;
	}

	/* Disallow recursive types */
	if (a->type == Tyvar && b->type != Tyvar) {
		if (occursin(a, b))
			fatal(ctx, "%s occurs within %s, leading to infinite type near %s\n",
				tystr(a), tystr(b), ctxstr(ctx));
	}

	r = NULL;
	if (a->type == Tyvar || tyeq(a, b)) {
		tytab[a->tid] = b;
		if (ctx) {
			marksrc(a, ctx->loc);
			marksrc(b, ctx->loc);
		}
	}
	if (a->type == Tyvar) {
		ea = basetype(a);
		eb = basetype(b);
		if (ea && eb)
			unify(ctx, ea, eb);
		r = b;
	}

	if (a->type == Tyarray && b->type == Tyarray) {
		checksize(ctx, a, b);
	}

	/* if the tyrank of a is 0 (ie, a raw tyvar), just unify.
	 * Otherwise, match up subtypes. */
	if (a->type == b->type && a->type != Tyvar) {
		if (a->type == Tyname)
			if (!nameeq(a->name, b->name))
				typeerror(a, b, ctx, "incompatible types");
		if (hasargs(a) && hasargs(b)) {
			/* Only Tygeneric and Tyname should be able to unify. And they
			 * should have the same names for this to be true. */
			if (!nameeq(a->name, b->name))
				typeerror(a, b, ctx, NULL);
			if (a->narg != b->narg)
				typeerror(a, b, ctx, "incompatible parameter lists");
			for (i = 0; i < a->narg; i++)
				unify(ctx, a->arg[i], b->arg[i]);
			r = b;
		}
		if (a->nsub != b->nsub) {
			verifytraits(ctx, a, b);
			if (tybase(a)->type == Tyfunc)
				typeerror(a, b, ctx, "function arity mismatch");
			else
				typeerror(a, b, ctx, "subtype counts incompatible");
		}
		for (i = 0; i < b->nsub; i++)
			unify(ctx, a->sub[i], b->sub[i]);
		r = b;
	} else if (a->type != Tyvar) {
		typeerror(a, b, ctx, NULL);
	}
	mergetraits(ctx, a, b);
	if (a->isreflect || b->isreflect) {
		tagreflect(r);
		tagreflect(a);
		tagreflect(b);
	}
	membunify(ctx, a, b);

	/* if we have delayed types for a tyvar, transfer it over. */
	if (a->type == Tyvar && b->type == Tyvar) {
		if (hthas(delayed, a) && !hthas(delayed, b))
			htput(delayed, b, htget(delayed, a));
		else if (hthas(delayed, b) && !hthas(delayed, a))
			htput(delayed, a, htget(delayed, b));
	} else if (hthas(delayed, a)) {
		unify(ctx, htget(delayed, a), tybase(b));
	}

	return r;
}

/* Applies unifications to function calls.
 * Funciton application requires a slightly
 * different approach to unification. */
static void
unifycall(Node *n)
{
	size_t i;
	Type *ft;

	ft = type(n->expr.args[0]);
	if (ft->type == Tyvar) {
		/* the first arg is the function itself, so it shouldn't be counted */
		ft = mktyfunc(n->loc, &n->expr.args[1], n->expr.nargs - 1, mktyvar(n->loc));
		unify(n, ft, type(n->expr.args[0]));
	} else if (tybase(ft)->type != Tyfunc) {
		fatal(n, "calling uncallable type %s", tystr(ft));
	}
	/* first arg: function itself */
	for (i = 1; i < n->expr.nargs; i++) {
		if (i == ft->nsub)
			fatal(n, "%s arity mismatch (expected %zd args, got %zd)",
					ctxstr(n->expr.args[0]), ft->nsub - 1, n->expr.nargs - 1);

		if (ft->sub[i]->type == Tyvalist) {
			break;
		}
		unify(n->expr.args[0], ft->sub[i], type(n->expr.args[i]));
	}
	if (i < ft->nsub && ft->sub[i]->type != Tyvalist)
		fatal(n, "%s arity mismatch (expected %zd args, got %zd)",
				ctxstr(n->expr.args[0]), ft->nsub - 1, i - 1);
	settype(n, ft->sub[0]);
}

static void
unifyparams(Node *ctx, Type *a, Type *b)
{
	size_t i;

	/* The only types with unifiable params are Tyunres and Tyname.
	 * Tygeneric should always be freshened, and no other types have
	 * parameters attached.
	 *
	 * FIXME: Is it possible to have parameterized typarams? */
	if (a->type != Tyunres && a->type != Tyname)
		return;
	if (b->type != Tyunres && b->type != Tyname)
		return;

	if (a->narg != b->narg)
		fatal(ctx, "mismatched arg list sizes: %s with %s near %s", tystr(a), tystr(b),
				ctxstr(ctx));
	for (i = 0; i < a->narg; i++)
		unify(ctx, a->arg[i], b->arg[i]);
}

static Type *
initvar(Node *n, Node *s)
{
	Type *t, *u, *param;
	Tysubst *subst;

	if (s->decl.ishidden && !allowhidden)
		fatal(n, "attempting to refer to hidden decl %s", ctxstr(n));

	param = n->expr.param;
	if (param)
		tyresolve(param);
	if (s->decl.isgeneric) {
		subst = mksubst();
		if (param)
			substput(subst, s->decl.trait->param, param);
		pushenv(s->decl.env);
		t = tysubstmap(subst, tf(s->decl.type), s->decl.type);
		if (s->decl.trait && !param) {
			u = tf(s->decl.trait->param);
			param = substget(subst, u);
			if (!param)
				fatal(n, "ambiguous trait decl %s", ctxstr(s));
		}
		popenv(s->decl.env);
		substfree(subst);
	} else {
		t = s->decl.type;
	}
	n->expr.did = s->decl.did;
	n->expr.isconst = s->decl.isconst;
	if (param) {
		n->expr.param = param;
		delayedcheck(n);
	}
	if (s->decl.isgeneric && !ingeneric) {
		t = tyfreshen(NULL, t);
		addspecialization(n, curstab());
		if (t->type == Tyvar) {
			settype(n, mktyvar(n->loc));
			delayedcheck(n);
		} else {
			settype(n, t);
		}
	} else {
		settype(n, t);
	}
	return t;
}

/* Finds out if the member reference is actually
 * referring to a namespaced name, instead of a struct
 * member. If it is, it transforms it into the variable
 * reference we should have, instead of the Omemb expr
 * that we do have */
static Node *
checkns(Node *n, Node **ret)
{
	Node *var, *name, *nsname;
	Node **args;
	Stab *stab;
	Node *s;

	/* check that this is a namespaced declaration */
	args = n->expr.args;
	if (n->type != Nexpr || exprop(n) != Omemb || exprop(args[0]) != Ovar)
		return n;
	name = args[0]->expr.args[0];
	stab = getns(namestr(name));
	if (!stab || getdcl(curstab(), name))
		return n;

	/* substitute the namespaced name */
	nsname = mknsname(n->loc, namestr(name), namestr(args[1]));
	s = getdcl(stab, args[1]);
	if (!s)
		fatal(n, "undeclared var %s.%s", nsname->name.ns, nsname->name.name);
	var = mkexpr(n->loc, Ovar, nsname, NULL);
	var->expr.idx = n->expr.idx;
	initvar(var, s);
	*ret = var;
	return var;
}

static void
inferstruct(Node *n, int *isconst)
{
	size_t i;

	*isconst = 1;
	/* we want to check outer nodes before inner nodes when unifying nested structs */
	delayedcheck(n);
	for (i = 0; i < n->expr.nargs; i++) {
		infernode(&n->expr.args[i], NULL, NULL);
		if (!n->expr.args[i]->expr.isconst)
			*isconst = 0;
	}
	settype(n, mktyvar(n->loc));
}

static int64_t
arraysize(Node *n)
{
	int64_t sz, off, i;
	Node **args, *idx;

	sz = 0;
	args = n->expr.args;
	for (i = 0; i < n->expr.nargs; i++) {
		if (args[i]->expr.idx) {
			args[i]->expr.idx = fold(args[i]->expr.idx, 1);
			idx = args[i]->expr.idx;
			if (exprop(idx) != Olit)
				fatal(idx, "nonconstant array initializer index near %s\n",
					ctxstr(idx));
			if (idx->expr.args[0]->lit.littype == Lchr)
				off = idx->expr.args[0]->lit.chrval;
			else if (idx->expr.args[0]->lit.littype == Lint)
				off = idx->expr.args[0]->lit.intval;
			else
				fatal(idx, "noninteger array initializer index near %s\n",
					ctxstr(idx));
			if (off >= sz)
				sz = off + 1;
		} else {
			sz++;
		}
	}
	return sz;
}

static void
inferarray(Node *n, int *isconst)
{
	size_t i;
	Type *t;
	Node *len;

	*isconst = 1;
	len = mkintlit(n->loc, arraysize(n));
	t = mktyarray(n->loc, mktyvar(n->loc), len);
	for (i = 0; i < n->expr.nargs; i++) {
		infernode(&n->expr.args[i], NULL, NULL);
		unify(n, t->sub[0], type(n->expr.args[i]));
		if (!n->expr.args[i]->expr.isconst)
			*isconst = 0;
	}
	settype(n, t);
}

static void
infertuple(Node *n, int *isconst)
{
	Type **types;
	size_t i;

	*isconst = 1;
	types = xalloc(sizeof(Type *) * n->expr.nargs);
	for (i = 0; i < n->expr.nargs; i++) {
		infernode(&n->expr.args[i], NULL, NULL);
		n->expr.isconst = n->expr.isconst && n->expr.args[i]->expr.isconst;
		types[i] = type(n->expr.args[i]);
	}
	*isconst = n->expr.isconst;
	settype(n, mktytuple(n->loc, types, n->expr.nargs));
}

static void
inferucon(Node *n, int *isconst)
{
	Ucon *uc;
	Type *t;

	*isconst = 1;
	uc = uconresolve(n);
	/* Hackety hack hack.
	 * the types in a generic union may be bound from the tyname that
	 * defined it, which is not accessible here.
	 *
	 * To make it compile, for now, we just bind the types in here.
	 */
	t = uc->utype;
	if (t->type != Tyunion)
		t = t->sub[0];
	assert(t->type == Tyunion);
	t = tysubst(tf(t), t);
	uc = tybase(t)->udecls[uc->id];
	if (uc->etype) {
		inferexpr(&n->expr.args[1], NULL, NULL);
		unify(n, uc->etype, type(n->expr.args[1]));
		*isconst = n->expr.args[1]->expr.isconst;
	}
	settype(n, delayeducon(uc->utype));
}

static void
inferpat(Node **np, Node *val, Node ***bind, size_t *nbind)
{
	size_t i;
	Node **args;
	Node *s, *n;
	Stab *ns;
	Type *t;

	n = *np;
	n = checkns(n, np);
	n->expr.ispat = 1;
	args = n->expr.args;
	for (i = 0; i < n->expr.nargs; i++)
		if (args[i]->type == Nexpr) {
			inferpat(&args[i], val, bind, nbind);
		}
	switch (exprop(n)) {
	case Otup:
	case Ostruct:
	case Oarr:
	case Olit:
	case Omemb:
	case Olor:
		infernode(np, NULL, NULL);
		break;
		/* arithmetic expressions just need to be constant */
	case Oneg:
	case Oadd:
	case Osub:
	case Omul:
	case Odiv:
	case Obsl:
	case Obsr:
	case Oband:
	case Obor:
	case Obxor:
	case Obnot:
		infernode(np, NULL, NULL);
		if (!n->expr.isconst)
			fatal(n, "matching against non-constant expression near %s", ctxstr(n));
		break;
	case Oucon:
		inferucon(n, &n->expr.isconst);
		break;
	case Ovar:
		ns = curstab();
		if (args[0]->name.ns)
			ns = getns(args[0]->name.ns);
		s = getdcl(ns, args[0]);
		if (s && !s->decl.ishidden) {
			if (s->decl.isgeneric)
				t = tysubst(s->decl.type, s->decl.type);
			else if (s->decl.isconst)
				t = s->decl.type;
			else
				fatal(n, "pattern shadows variable declared on %s:%d near %s",
						fname(s->loc), lnum(s->loc), ctxstr(s));
		} else {
			/* Scan the already collected bound variables in the pattern of this match case.
			 * If a bound variable with the same name is found, assign the variable the existing decl.
			 * Otherwise, create a new decl for the variable */
			s = NULL;
			for (i = 0; !s && i < *nbind; i++)
				if (nameeq(args[0], (*bind)[i]->decl.name))
					s = (*bind)[i];

			if (s) {
				t = s->decl.type;
			} else {
				t = mktyvar(n->loc);
				s = mkdecl(n->loc, n->expr.args[0], t);
				s->decl.init = val;
				settype(n, t);
				lappend(bind, nbind, s);
			}
		}
		settype(n, t);
		n->expr.did = s->decl.did;
		n->expr.isconst = s->decl.isconst;
		break;
	case Oaddr:
		infernode(np, NULL, NULL);
		break;
	case Ogap:
		infernode(np, NULL, NULL);	break;
	default: fatal(n, "invalid pattern"); break;
	}
}

void
addbindings(Node *n, Node **bind, size_t nbind)
{
	size_t i;

	/* order of binding shouldn't matter, so push them into the block
	 * in reverse order. */
	for (i = 0; i < nbind; i++) {
		putdcl(n->block.scope, bind[i]);
		linsert(&n->block.stmts, &n->block.nstmts, 0, bind[i]);
	}
}

static void
infersub(Node *n, Type *ret, int *sawret, int *exprconst)
{
	Node **args;
	size_t i, nargs;
	int isconst;

	/* Ovar has no subexpressions */
	if (exprop(n) == Ovar)
		return;
	args = n->expr.args;
	nargs = n->expr.nargs;
	isconst = 1;
	for (i = 0; i < nargs; i++) {
		/* Nlit, Nvar, etc should not be inferred as exprs */
		if (args[i]->type == Nexpr) {
			/* Omemb can sometimes resolve to a namespace. We have to check
			 * this. Icky. */
			checkns(args[i], &args[i]);
			inferexpr(&args[i], ret, sawret);
			isconst = isconst && args[i]->expr.isconst;
		}
	}
	if (opispure[exprop(n)])
		n->expr.isconst = isconst;
	*exprconst = n->expr.isconst;
}

static void
inferexpr(Node **np, Type *ret, int *sawret)
{
	Node **args;
	size_t i, nargs;
	Node *s, *n;
	Type *t, *b;
	int isconst;
	Stab *ns;

	n = *np;
	assert(n->type == Nexpr);
	args = n->expr.args;
	nargs = n->expr.nargs;
	infernode(&n->expr.idx, NULL, NULL);
	n = checkns(n, np);
	switch (exprop(n)) {
	case Oauto:	/* @a -> @a */
		infersub(n, ret, sawret, &isconst);
		t = type(args[0]);
		constrain(n, t, traittab[Tcdisp]);
		n->expr.isconst = isconst;
		settype(n, t);
		break;
		/* all operands are same type */
	case Oadd:	/* @a + @a -> @a */
	case Osub:	/* @a - @a -> @a */
	case Omul:	/* @a * @a -> @a */
	case Odiv:	/* @a / @a -> @a */
	case Oneg:	/* -@a -> @a */
	case Oaddeq:	/* @a += @a -> @a */
	case Osubeq:	/* @a -= @a -> @a */
	case Omuleq:	/* @a *= @a -> @a */
	case Odiveq:	/* @a /= @a -> @a */
		infersub(n, ret, sawret, &isconst);
		t = type(args[0]);
		constrain(n, t, traittab[Tcnum]);
		isconst = args[0]->expr.isconst;
		for (i = 1; i < nargs; i++) {
			isconst = isconst && args[i]->expr.isconst;
			t = unify(n, t, type(args[i]));
		}
		n->expr.isconst = isconst;
		settype(n, t);
		break;
	case Omod:	/* @a % @a -> @a */
	case Obor:	/* @a | @a -> @a */
	case Oband:	/* @a & @a -> @a */
	case Obxor:	/* @a ^ @a -> @a */
	case Obsl:	/* @a << @a -> @a */
	case Obsr:	/* @a >> @a -> @a */
	case Obnot:	/* ~@a -> @a */
	case Opreinc:	/* ++@a -> @a */
	case Opredec:	/* --@a -> @a */
	case Opostinc:	/* @a++ -> @a */
	case Opostdec:	/* @a-- -> @a */
	case Omodeq:	/* @a %= @a -> @a */
	case Oboreq:	/* @a |= @a -> @a */
	case Obandeq:	/* @a &= @a -> @a */
	case Obxoreq:	/* @a ^= @a -> @a */
	case Obsleq:	/* @a <<= @a -> @a */
	case Obsreq:	/* @a >>= @a -> @a */
		infersub(n, ret, sawret, &isconst);
		t = type(args[0]);
		constrain(n, t, traittab[Tcnum]);
		constrain(n, t, traittab[Tcint]);
		isconst = args[0]->expr.isconst;
		for (i = 1; i < nargs; i++) {
			isconst = isconst && args[i]->expr.isconst;
			t = unify(n, t, type(args[i]));
		}
		n->expr.isconst = isconst;
		settype(n, t);
		break;
	case Oasn:	/* @a = @a -> @a */
		infersub(n, ret, sawret, &isconst);
		t = type(args[0]);
		for (i = 1; i < nargs; i++)
			t = unify(n, t, type(args[i]));
		settype(n, t);
		if (args[0]->expr.isconst)
			fatal(n, "attempting to assign constant \"%s\"", ctxstr(args[0]));
		break;

		/* operands same type, returning bool */
	case Olor:	/* @a || @b -> bool */
		if (n->expr.ispat) {
			infersub(n, ret, sawret, &isconst);
			t = type(args[0]);
			for (i = 1; i < nargs; i++)
				t = unify(n, t, type(args[i]));
			n->expr.isconst = isconst;
			settype(n, t);
			break;
		}
	case Oland:	/* @a && @b -> bool */
	case Oeq:	/* @a == @a -> bool */
	case One:	/* @a != @a -> bool */
	case Ogt:	/* @a > @a -> bool */
	case Oge:	/* @a >= @a -> bool */
	case Olt:	/* @a < @a -> bool */
	case Ole:	/* @a <= @b -> bool */
		infersub(n, ret, sawret, &isconst);
		t = type(args[0]);
		for (i = 1; i < nargs; i++)
			unify(n, t, type(args[i]));
		settype(n, mktype(Zloc, Tybool));
		break;

	case Olnot:	/* !bool -> bool */
		infersub(n, ret, sawret, &isconst);
		t = unify(n, type(args[0]), mktype(Zloc, Tybool));
		settype(n, t);
		break;

		/* reach into a type and pull out subtypes */
	case Oaddr:	/* &@a -> @a* */
		infersub(n, ret, sawret, &isconst);
		settype(n, mktyptr(n->loc, type(args[0])));
		break;
	case Oderef:	/* @a# ->  @a */
		infersub(n, ret, sawret, &isconst);
		t = unify(n, type(args[0]), mktyptr(n->loc, mktyvar(n->loc)));
		settype(n, t->sub[0]);
		break;
	case Oidx:	/* @a[@b::tcint] -> @a */
		infersub(n, ret, sawret, &isconst);
		b = mktyvar(n->loc);
		t = mktyvar(n->loc);
		t->seqaux = b;
		unify(n, type(args[0]), t);
		constrain(n, type(args[0]), traittab[Tcidx]);
		constrain(n, type(args[1]), traittab[Tcint]);
		settype(n, b);
		break;
	case Oslice:	/* @a[@b::tcint,@b::tcint] -> @a[,] */
		infersub(n, ret, sawret, &isconst);
		b = mktyvar(n->loc);
		t = mktyvar(n->loc);
		t->seqaux = b;
		unify(n, type(args[0]), t);
		constrain(n, type(args[1]), traittab[Tcint]);
		constrain(n, type(args[1]), traittab[Tcnum]);
		constrain(n, type(args[2]), traittab[Tcint]);
		constrain(n, type(args[2]), traittab[Tcnum]);
		settype(n, mktyslice(n->loc, b));
		break;

		/* special cases */
	case Otupmemb:	/* @a.N -> @b, verify type(@a.N)==@b later */
	case Omemb:	/* @a.Ident -> @b, verify type(@a.Ident)==@b later */
		infersub(n, ret, sawret, &isconst);
		settype(n, mktyvar(n->loc));
		delayedcheck(n);
		break;
	case Osize:	/* sizeof(@a) -> size */
		settype(n, mktylike(n->loc, Tyuint));
		inferdecl(args[0]);
		n->expr.isconst = 1;
		break;
	case Ocall:	/* (@a, @b, @c, ... -> @r)(@a, @b, @c, ...) -> @r */
		infersub(n, ret, sawret, &isconst);
		unifycall(n);
		break;
	case Ocast:	/* (@a : @b) -> @b */
		infersub(n, ret, sawret, &isconst);
		delayedcheck(n);
		break;
	case Oret:	/* -> @a -> void */
		infersub(n, ret, sawret, &isconst);
		if (!ret)
			fatal(n, "returns are not valid near %s", ctxstr(n));
		if (sawret)
			*sawret = 1;
		t = unify(n, ret, type(args[0]));
		settype(n, t);
		break;
	case Obreak:
	case Ocontinue:
		/* nullary: nothing to infer. */
		settype(n, mktype(Zloc, Tyvoid));
		break;
	case Ojmp:	/* goto void* -> void */
		if (args[0]->type == Nlit && args[0]->lit.littype == Llbl)
			args[0] = getlbl(curstab(), args[0]->loc, args[0]->lit.lblname);
		infersub(n, ret, sawret, &isconst);
		settype(n, mktype(Zloc, Tyvoid));
		break;
	case Ovar:	/* a:@a -> @a */
		infersub(n, ret, sawret, &isconst);
		/* if we created this from a namespaced var, the type should be
		 * set, and the normal lookup is expected to fail. Since we're
		 * already done with this node, we can just return. */
		if (n->expr.type)
			return;
		ns = curstab();
		if (args[0]->name.ns)
			ns = getns(args[0]->name.ns);
		s = getdcl(ns, args[0]);
		if (!s)
			fatal(n, "undeclared var %s", ctxstr(args[0]));
		if (n->expr.param && !s->decl.trait)
			fatal(n, "var %s must refer to a trait decl", ctxstr(args[0]));
		pushenv(s->decl.env);
		initvar(n, s);
		popenv(s->decl.env);
		break;
	case Ogap:	/* _ -> @a */
		if (n->expr.type)
			return;
		n->expr.type = mktyvar(n->loc);
		break;
	case Oucon:	inferucon(n, &n->expr.isconst);	break;
	case Otup:	infertuple(n, &n->expr.isconst);	break;
	case Ostruct:	inferstruct(n, &n->expr.isconst);	break;
	case Oarr:	inferarray(n, &n->expr.isconst);	break;
	case Olit:	/* <lit>:@a::tyclass -> @a */
		infersub(n, ret, sawret, &isconst);
		switch (args[0]->lit.littype) {
		case Lfunc:
			infernode(&args[0]->lit.fnval, NULL, NULL);
			/* FIXME: env capture means this is non-const */
			n->expr.isconst = 1;
			break;
		case Llbl:
			s = getlbl(curstab(), args[0]->loc, args[0]->lit.lblname);
			if (!s)
				fatal(n, "unable to find label %s in function scope\n", args[0]->lit.lblname);
			*np = s;
			break;
		default:
			n->expr.isconst = 1;
			break;
		}
		settype(n, type(args[0]));
		break;
	case Otern:
		infersub(n, NULL, sawret, &isconst);
		unify(n, type(args[0]), mktype(n->loc, Tybool));
		unify(n, type(args[1]), type(args[2]));
		settype(n, type(args[1]));
		break;
	case Oundef:
		infersub(n, ret, sawret, &isconst);
		settype(n, mktype(n->loc, Tyvoid));
		break;
	case Odef:
	case Odead:
		   n->expr.type = mktype(n->loc, Tyvoid);
		   break;
	/* unexpected in frontend */
	case Obad: case Ocjmp: case Ovjmp: case Oset: case Oslbase: case Osllen:
	case Outag: case Ocallind: case Oblit: case Oclear: case Oudata: case Otrunc:
	case Oswiden: case Ozwiden: case Oint2flt: case Oflt2int: case Oflt2flt:
	case Ofadd: case Ofsub: case Ofmul: case Ofdiv: case Ofneg: case Ofeq:
	case Ofne: case Ofgt: case Ofge: case Oflt: case Ofle: case Oueq: case Oune:
	case Ougt: case Ouge: case Oult: case Oule: case Otupget: case Numops:
		die("Should not see %s in fe", opstr[exprop(n)]);
		break;
	}
}

static void
inferfunc(Node *n)
{
	size_t i;
	int sawret;

	sawret = 0;
	for (i = 0; i < n->func.nargs; i++)
		infernode(&n->func.args[i], NULL, NULL);
	infernode(&n->func.body, n->func.type->sub[0], &sawret);
	/* if there's no return stmt in the function, assume void ret */
	if (!sawret)
		unify(n, type(n)->sub[0], mktype(Zloc, Tyvoid));
}

static void
specializeimpl(Node *n)
{
	Node *dcl, *proto, *name, *sym;
	Tysubst *subst;
	Type *ty;
	Trait *tr;
	size_t i, j;
	int generic;
	char *traitns;

	tr = gettrait(curstab(), n->impl.traitname);
	if (!tr)
		fatal(n, "no trait %s\n", namestr(n->impl.traitname));
	n->impl.trait = tr;
	traitns = tr->name->name.ns;

	dcl = NULL;
	if (n->impl.naux != tr->naux)
		fatal(n, "%s incompatibly specialized with %zd types instead of %zd types",
			namestr(n->impl.traitname), n->impl.naux, tr->naux);
	n->impl.type = tf(n->impl.type);
	pushenv(n->impl.env);
	for (i = 0; i < n->impl.naux; i++)
		n->impl.aux[i] = tf(n->impl.aux[i]);
	for (i = 0; i < n->impl.ndecls; i++) {
		/* look up the prototype */
		proto = NULL;
		dcl = n->impl.decls[i];

		/*
		   since the decls in an impl are not installed in a namespace,
		   their names are not updated when we call updatens() on the
		   symbol table. Because we need to do namespace dependent
		   comparisons for specializing, we need to set the namespace
		   here.
		*/
		if (file.globls->name)
			setns(dcl->decl.name, traitns);
		for (j = 0; j < tr->nproto; j++) {
			if (nsnameeq(dcl->decl.name, tr->proto[j]->decl.name)) {
				proto = tr->proto[j];
				break;
			}
		}
		if (!proto)
			fatal(n, "declaration %s missing in %s, near %s", namestr(dcl->decl.name),
					namestr(tr->name), ctxstr(n));

		/* infer and unify types */
		pushenv(proto->decl.env);
		verifytraits(n, tr->param, n->impl.type);
		subst = mksubst();
		substput(subst, tr->param, n->impl.type);
		for (j = 0; j < tr->naux; j++)
			substput(subst, tr->aux[j], n->impl.aux[j]);
		ty = tyspecialize(type(proto), subst, delayed);
		substfree(subst);
		popenv(proto->decl.env);

		generic = hasparams(ty);
		if (generic)
			ingeneric++;

		inferdecl(dcl);
		unify(n, type(dcl), ty);

		/* and put the specialization into the global stab */
		name = genericname(proto, n->impl.type, ty);
		sym = getdcl(file.globls, name);
		if (sym)
			fatal(n, "trait %s already specialized with %s on %s:%d",
				namestr(tr->name), tystr(n->impl.type),
				fname(sym->loc), lnum(sym->loc));
		dcl->decl.name = name;
		putdcl(file.globls, dcl);
		htput(proto->decl.impls, n->impl.type, dcl);
		dcl->decl.isconst = 1;
		if (ty->type == Tygeneric || hasparams(ty)) {
			dcl->decl.isgeneric = 1;
			lappend(&proto->decl.gimpl, &proto->decl.ngimpl, dcl);
			lappend(&proto->decl.gtype, &proto->decl.ngtype, ty);
		}
		dcl->decl.vis = tr->vis;
		lappend(&impldecl, &nimpldecl, dcl);

		if (generic)
			ingeneric--;
	}
	popenv(n->impl.env);
}

static void
inferdecl(Node *n)
{
	Type *t;

	t = tf(decltype(n));
	if (t->type == Tygeneric && !n->decl.isgeneric) {
		t = tyfreshen(NULL, t);
		unifyparams(n, t, decltype(n));
	}
	settype(n, t);
	if (n->decl.init) {
		inferexpr(&n->decl.init, NULL, NULL);
		unify(n, type(n), type(n->decl.init));
		if (n->decl.isconst && !n->decl.init->expr.isconst)
			fatal(n, "non-const initializer for \"%s\"", ctxstr(n));
	}
}

static void
inferstab(Stab *s)
{
	void **k;
	size_t n, i;
	Node *dcl;
	Type *t;
	void *se;

	k = htkeys(s->dcl, &n);
	se = curenv();
	for (i = 0; i < n; i++) {
		dcl = htget(s->dcl, k[i]);
		pushenv(dcl->decl.env);
		tf(type(dcl));
		popenv(dcl->decl.env);
		assert(se == curenv());
	}
	free(k);

	k = htkeys(s->ty, &n);
	for (i = 0; i < n; i++) {
		t = gettype(s, k[i]);
		if (!t)
			fatal(k[i], "undefined type %s", namestr(k[i]));
		t = tysearch(t);
		setsuperenv(t->env, curenv());
		pushenv(t->env);
		tyresolve(t);
		popenv(t->env);
		updatetype(s, k[i], t);
	}
	free(k);
}

static void
infernode(Node **np, Type *ret, int *sawret)
{
	size_t i, nbound;
	Node **bound, *n, *pat;
	Type *t, *b, *e;

	n = *np;
	if (!n)
		return;
	if (n->inferred)
		return;
	n->inferred = 1;
	switch (n->type) {
	case Ndecl:
		if (n->decl.isgeneric)
			ingeneric++;
		indentdepth++;
		setsuperenv(n->decl.env, curenv());
		pushenv(n->decl.env);
		inferdecl(n);
		if (hasparams(type(n)) && !ingeneric)
			fatal(n, "generic type in non-generic near %s", ctxstr(n));
		popenv(n->decl.env);
		indentdepth--;
		if (n->decl.isgeneric)
			ingeneric--;
		break;
	case Nblock:
		setsuper(n->block.scope, curstab());
		pushstab(n->block.scope);
		inferstab(n->block.scope);
		for (i = 0; i < n->block.nstmts; i++)
			infernode(&n->block.stmts[i], ret, sawret);
		popstab();
		break;
	case Nifstmt:
		infernode(&n->ifstmt.cond, NULL, sawret);
		infernode(&n->ifstmt.iftrue, ret, sawret);
		infernode(&n->ifstmt.iffalse, ret, sawret);
		unify(n, type(n->ifstmt.cond), mktype(n->loc, Tybool));
		break;
	case Nloopstmt:
		setsuper(n->loopstmt.scope, curstab());
		pushstab(n->loopstmt.scope);
		infernode(&n->loopstmt.init, ret, sawret);
		infernode(&n->loopstmt.cond, NULL, sawret);
		infernode(&n->loopstmt.step, ret, sawret);
		infernode(&n->loopstmt.body, ret, sawret);
		unify(n, type(n->loopstmt.cond), mktype(n->loc, Tybool));
		popstab();
		break;
	case Niterstmt:
		bound = NULL;
		nbound = 0;

		inferpat(&n->iterstmt.elt, NULL, &bound, &nbound);
		addbindings(n->iterstmt.body, bound, nbound);

		infernode(&n->iterstmt.seq, NULL, sawret);
		infernode(&n->iterstmt.body, ret, sawret);

		e = type(n->iterstmt.elt);
		t = type(n->iterstmt.seq);
		constrain(n, t, traittab[Tciter]);
		b = basetype(t);
		if (b && t->type != Typtr)
			unify(n, e, b);
		else
			t->seqaux = e;
		delayedcheck(n);
		break;
	case Nmatchstmt:
		infernode(&n->matchstmt.val, NULL, sawret);
		for (i = 0; i < n->matchstmt.nmatches; i++) {
			infernode(&n->matchstmt.matches[i], ret, sawret);
			pat = n->matchstmt.matches[i]->match.pat;
			unify(pat, type(n->matchstmt.val),
					type(n->matchstmt.matches[i]->match.pat));
		}
		break;
	case Nmatch:
		bound = NULL;
		nbound = 0;
		inferpat(&n->match.pat, NULL, &bound, &nbound);
		addbindings(n->match.block, bound, nbound);
		infernode(&n->match.block, ret, sawret);
		break;
	case Nexpr:
		inferexpr(np, ret, sawret);
		break;
	case Nfunc:
		setsuper(n->func.scope, curstab());
		pushstab(n->func.scope);
		setsuperenv(n->func.env, curenv());
		pushenv(n->func.env);
		inferstab(n->func.scope);
		inferfunc(n);
		popenv(n->func.env);
		popstab();
		break;
	case Nimpl:
		specializeimpl(n);
		break;
	case Nlit:
	case Nname:
	case Nuse:
		break;
	case Nnone:
		die("Nnone should not be seen as node type!");
		break;
	}
}

/* returns the final type for t, after all unification
 * and default constraint selections */
static Type *
tyfix(Node *ctx, Type *orig, int noerr)
{
	static Type *tyint, *tyflt;
	static Bitset *intset, *fltset;
	Type *t, *d, *base;
	Tyenv *env;
	vlong val;
	size_t i;
	char buf[1024];

	if (!tyint) {
		tyint = mktype(Zloc, Tyint);
		intset = mkbs();
		traitsfor(tyint, intset);
	}
	if (!tyflt) {
		tyflt = mktype(Zloc, Tyflt64);
		fltset = mkbs();
		traitsfor(tyflt, fltset);
	}

	t = tysearch(tf(orig));
	env = t->env;
	if (env)
		pushenv(env);
	base = orig->seqaux;
	/* Process delayed type mappings. */
	if (orig->type == Tyvar && hthas(delayed, orig)) {
		d = htget(delayed, orig);
		if (t->type == Tyvar) {
			/* tyvar is guaranteed to unify error-free */
			unify(ctx, t, d);
			t = tf(t);
		} else if (tybase(t)->type != d->type && !noerr) {
			fatal(ctx, "type %s not compatible with %s near %s\n",
			    tystr(t), tystr(d), ctxstr(ctx));
		}
	}

	/* Default the type */
	if (t->type == Tyvar && t->trneed) {
		if (bsissubset(t->trneed, intset))
			t = tyint;
		else if (bsissubset(t->trneed, fltset))
			t = tyflt;
	}

	if (!t->fixed) {
		t->fixed = 1;
		switch(t->type) {
		case Tyarray:
			if (t->type == Tyarray && t->asize)
				t->asize = fold(t->asize, 1);
			if (t->asize && getintlit(t->asize, &val) && val < 0)
				fatal(t->asize, "negative array size %lld\n", val);
			typesub(t->asize, noerr);
			break;
		case Tystruct:
			inaggr++;
			for (i = 0; i < t->nmemb; i++)
				typesub(t->sdecls[i], noerr);
			inaggr--;
			break;
		case Tyunion:
			for (i = 0; i < t->nmemb; i++) {
				if (t->udecls[i]->etype) {
					tyresolve(t->udecls[i]->etype);
					t->udecls[i]->etype =
						tyfix(ctx, t->udecls[i]->etype, noerr);
				}
			}
			break;
		case Tyname:
			for (i = 0; i < t->narg; i++)
				t->arg[i] = tyfix(ctx, t->arg[i], noerr);
			break;
		default:
			break;
		}
	}

	for (i = 0; i < t->nsub; i++)
		t->sub[i] = tyfix(ctx, t->sub[i], noerr);

	if (t->type == Tyvar && !noerr)
		fatal(ctx, "underconstrained type %s near %s", tyfmt(buf, 1024, t), ctxstr(ctx));
	if (base) {
		if (base != orig)
			base = tyfix(ctx, base, noerr);
		t->seqaux = base;
	}
	if (env)
		popenv(env);
	return t;
}

static void
checkcast(Node *n, Postcheck ***pc, size_t *npc)
{
	/* FIXME: actually verify the casts. Right now, it's ok to leave thi
	 * unimplemented because bad casts get caught by the backend. */
}

static void
infercompn(Node *n, Postcheck ***post, size_t *npost)
{
	Node *aggr, *memb, **nl;
	int found, ismemb;
	Postcheck *pc;
	uvlong idx;
	size_t i;
	Type *t;

	aggr = n->expr.args[0];
	memb = n->expr.args[1];
	ismemb = n->expr.op == Omemb;

	found = 0;
	t = tybase(tf(type(aggr)));
	/* all array-like types have a fake "len" member that we emulate */
	if (ismemb && (t->type == Tyslice || t->type == Tyarray)) {
		if (!strcmp(namestr(memb), "len")) {
			constrain(n, type(n), traittab[Tcnum]);
			constrain(n, type(n), traittab[Tcint]);
			found = 1;
		}
	} else if (ismemb && t->type == Tyunion) {
		if (!strcmp(namestr(memb), "tag")) {
			constrain(n, type(n), traittab[Tcnum]);
			constrain(n, type(n), traittab[Tcint]);
			found = 1;
		}
	} else {
		if (tybase(t)->type == Typtr)
			t = tybase(tf(t->sub[0]));
		if (tybase(t)->type == Tyvar) {
			pc = xalloc(sizeof(Postcheck));
			pc->node = n;
			pc->stab = curstab();
			pc->env = curenv();
			lappend(post, npost, pc);
			return;
		}
		if (ismemb) {
			/*
			 * aggregate types for the member, and unify the expression with the
			 * member type; ie:
			 *
			 *	 x: aggrtype	y : memb in aggrtype
			 *	 ---------------------------------------
			 *			   x.y : membtype
			 */
			if (tybase(t)->type != Tystruct)
				fatal(n, "type %s does not support member operators near %s",
						tystr(t), ctxstr(n));
			nl = t->sdecls;
			for (i = 0; i < t->nmemb; i++) {
				if (!strcmp(namestr(memb), declname(nl[i]))) {
					unify(n, type(n), decltype(nl[i]));
					found = 1;
					break;
				}
			}
		} else {
			/* tuple access; similar to the logic for member accesses */
			if (tybase(t)->type != Tytuple)
				fatal(n, "type %s does not support tuple access operators near %s",
						tystr(t), ctxstr(n));
			assert(memb->type == Nlit);
			idx = memb->lit.intval;
			if (idx >= t->nsub)
				fatal(n, "cannot access element %llu of a tuple of type %s near %s",
						idx, tystr(t), ctxstr(n));
			unify(n, type(n), t->sub[idx]);
			found = 1;
		}
	}
	if (!found)
		fatal(aggr, "type %s has no member \"%s\" near %s", tystr(type(aggr)),
				ctxstr(memb), ctxstr(aggr));
}

static void
checkstruct(Node *n, Postcheck ***post, size_t *npost)
{
	Type *t, *et;
	Node *val, *name;
	size_t i, j;
	Postcheck *pc;

	t = tybase(tf(n->lit.type));
	/*
	 * If we haven't inferred the type, and it's inside another struct,
	 * we'll eventually get to it.
	 *
	 * If, on the other hand, it is genuinely underspecified, we'll give
	 * a better error on it later.
	 */
	if (t->type == Tyvar)
		return;
	if (t->type != Tystruct)
		fatal(n, "struct literal is used as non struct type %s", tystr(n->lit.type));
	for (i = 0; i < n->expr.nargs; i++) {
		val = n->expr.args[i];
		name = val->expr.idx;

		et = NULL;
		for (j = 0; j < t->nmemb; j++) {
			if (!strcmp(namestr(t->sdecls[j]->decl.name), namestr(name))) {
				et = type(t->sdecls[j]);
				break;
			}
		}

		if (et) {
			unify(val, et, type(val));
		} else {
			assert(post != NULL);
			pc = xalloc(sizeof(Postcheck));
			pc->node = n;
			pc->stab = curstab();
			pc->env = curenv();
			lappend(post, npost, pc);
			return;
		}
	}
}

static void
checkvar(Node *n, Postcheck ***post, size_t *npost)
{
	Node *proto, *dcl;
	Type *ty;

	proto = decls[n->expr.did];
	ty = NULL;
	dcl = NULL;
	pushenv(proto->decl.env);
	if (n->expr.param)
		dcl = htget(proto->decl.impls, tf(n->expr.param));
	if (dcl)
		ty = dcl->decl.type;
	if (!ty)
		ty = tyfreshen(NULL, type(proto));
	unify(n, type(n), ty);
	popenv(proto->decl.env);
}

static void
fixiter(Node *n, Type *ty, Type *base)
{
	size_t i, bestidx;
	int r, bestrank;
	Type *b, *t, *orig;
	Tysubst *ts;
	Node *impl;

	ty = tysearch(ty);
	b = ty->seqaux;
	if (!b)
		return;
	bestrank = -1;
	bestidx = 0;
	for (i = 0; i < nimpltab; i++) {
		if (impltab[i]->impl.trait != traittab[Tciter])
			continue;
		r = tymatchrank(impltab[i]->impl.type, ty);
		if (r > bestrank) {
			bestrank = r;
			bestidx = i;
		}
	}
	if (bestrank >= 0) {
		impl = impltab[bestidx];
		orig = impl->impl.type;
		t = tf(impl->impl.aux[0]);
		ts = mksubst();
		for (i = 0; i < ty->narg; i++)
			substput(ts, tf(orig->arg[i]), ty->arg[i]);
		t = tyfreshen(ts, t);
		substfree(ts);
		unify(n, t, base);
	}
}

static void
postcheckpass(Postcheck ***post, size_t *npost)
{
	size_t i;
	Node *n;

	for (i = 0; i < npostcheck; i++) {
		n = postcheck[i]->node;
		pushstab(postcheck[i]->stab);
		pushenv(postcheck[i]->env);
		if (n->type == Nexpr) {
			switch (exprop(n)) {
			case Otupmemb:
			case Omemb:	infercompn(n, post, npost);	break;
			case Ocast:	checkcast(n, post, npost);	break;
			case Ostruct:	checkstruct(n, post, npost);	break;
			case Ovar:	checkvar(n, post, npost);	break;
			default:
				die("should not see %s in postcheck\n", opstr[exprop(n)]);
				break;
			}
		} else if (n->type == Niterstmt) {
			fixiter(n, type(n->iterstmt.seq), type(n->iterstmt.elt));
		}
		popenv(postcheck[i]->env);
		popstab();
	}
}

static void
postinfer(void)
{
	Postcheck **post;
	size_t npost, nlast;

	/* Iterate until we reach a fixpoint. */
	nlast = 0;
	while (1) {
		post = NULL;
		npost = 0;
		postcheckpass(&post, &npost);
		if (npost == nlast) {
			break;
		}
		nlast = npost;
	}
}

/* After inference, replace all
 * types in symbol tables with
 * the final computed types */
static void
stabsub(Stab *s)
{
	void **k;
	size_t n, i;
	Type *t;
	Node *d;
	char *dt;

	k = htkeys(s->ty, &n);
	for (i = 0; i < n; i++) {
		t = tysearch(gettype(s, k[i]));
		updatetype(s, k[i], t);
		tyfix(k[i], t, 0);
	}
	free(k);

	k = htkeys(s->dcl, &n);
	for (i = 0; i < n; i++) {
		d = getdcl(s, k[i]);
		pushenv(d->decl.env);
		d->decl.type = tyfix(d, d->decl.type, 0);
		popenv(d->decl.env);
		if (!d->decl.isconst && !d->decl.isgeneric)
			continue;
		if (d->decl.trait)
			continue;
		dt = "const";
		if (d->decl.isgeneric)
			dt = "generic";
		if (!d->decl.isimport && !d->decl.isextern && !d->decl.init)
			fatal(d, "non-extern %s \"%s\" has no initializer", dt, ctxstr(d));
	}
	free(k);
}

static void
checkrange(Node *n)
{
	Type *t;
	int64_t sval;
	uint64_t uval;
	static const int64_t svranges[][2] = {
		/* signed ints; allow one above max range for unary -'ve operator */
		[Tyint8]  = {-128LL, 128LL},
		[Tyint16] = {-32768LL, 32768LL},
		[Tyint32] = {-2147483648LL, 2147483648LL},
		[Tyint]	  = {-2147483648LL, 2147483648LL},
		[Tyint64] = {-9223372036854775808ULL, 9223372036854775807ULL},
	};

	static const uint64_t uvranges[][2] = {
		[Tybyte]   = {0, 255ULL},
		[Tyuint8]  = {0, 255ULL},
		[Tyuint16] = {0, 65535ULL},
		[Tyuint]   = {0, 4294967295ULL},
		[Tyuint32] = {0, 4294967295ULL},
		[Tyuint64] = {0, 18446744073709551615ULL},
		[Tychar]   = {0, 4294967295ULL},
	};

	/* signed types */
	t = type(n);
	if (t->type >= Tyint8 && t->type <= Tyint64) {
		sval = n->lit.intval;
		if (sval < svranges[t->type][0] || sval > svranges[t->type][1])
			fatal(n, "literal value %lld out of range for type \"%s\"", sval, tystr(t));
	} else if ((t->type >= Tybyte && t->type <= Tyuint64) || t->type == Tychar) {
		uval = n->lit.intval;
		if (uval < uvranges[t->type][0] || uval > uvranges[t->type][1])
			fatal(n, "literal value %llu out of range for type \"%s\"", uval, tystr(t));
	}
}

static int
initcompatible(Type *t)
{
	if (t->type != Tyfunc)
		return 0;
	if (t->nsub != 1)
		return 0;
	if (tybase(t->sub[0])->type != Tyvoid)
		return 0;
	return 1;
}

static int
maincompatible(Type *t)
{
	if (t->nsub > 2)
		return 0;
	if (tybase(t->sub[0])->type != Tyvoid)
		return 0;
	if (t->nsub == 2) {
		t = tybase(t->sub[1]);
		if (t->type != Tyslice)
			return 0;
		t = tybase(t->sub[0]);
		if (t->type != Tyslice)
			return 0;
		t = tybase(t->sub[0]);
		if (t->type != Tybyte)
			return 0;
	}
	return 1;
}

static void
verifyop(Node *n)
{
	Type *ty, *bty;
	Node *a;
	char *sa, *sb;
	size_t i, j;
	int found;

	ty = exprtype(n);
	bty = tybase(ty);
	switch (exprop(n)) {
	case Ostruct:
		if (bty->type != Tystruct)
			fatal(n, "wrong type for struct literal: %s\n", tystr(ty));
		for (i = 0; i < n->expr.nargs; i++) {
			found = 0;
			a = n->expr.args[i];
			for (j = 0; j < bty->nmemb; j++) {
				sa = namestr(a->expr.idx);
				sb = declname(bty->sdecls[j]);
				if (!strcmp(sa, sb))
					found = 1;
			}
			if (!found)
				fatal(n, "type %s has no member %s",
				    tystr(ty), namestr(a->expr.idx));
		}
		break;
	case Oarr:
		if (bty->type != Tyarray)
			fatal(n, "wrong type for struct literal: %s\n", tystr(ty));
		break;
	default:
		break;
	}
}

/* After type inference, replace all type
 * with the final computed type */
static void
typesub(Node *n, int noerr)
{
	char *name;
	size_t i;
	Node *l;

	if (!n)
		return;
	switch (n->type) {
	case Ndecl:
		pushenv(n->decl.env);
		settype(n, tyfix(n, type(n), noerr));
		if (n->decl.init)
			typesub(n->decl.init, noerr);
		if (streq(declname(n), "main"))
			if (!maincompatible(tybase(decltype(n))))
				fatal(n, "main must be (->void) or (byte[:][:] -> void), got %s",
						tystr(decltype(n)));
		name = declname(n);
		if (streq(name, "__init__") || streq(name, "__fini__"))
			if (!initcompatible(tybase(decltype(n))))
				fatal(n, "%s must be (->void), got %s", name, tystr(decltype(n)));
		popenv(n->decl.env);
		break;
	case Nblock:
		pushstab(n->block.scope);
		for (i = 0; i < n->block.nstmts; i++)
			typesub(n->block.stmts[i], noerr);
		popstab();
		break;
	case Nifstmt:
		typesub(n->ifstmt.cond, noerr);
		typesub(n->ifstmt.iftrue, noerr);
		typesub(n->ifstmt.iffalse, noerr);
		break;
	case Nloopstmt:
		typesub(n->loopstmt.cond, noerr);
		typesub(n->loopstmt.init, noerr);
		typesub(n->loopstmt.step, noerr);
		typesub(n->loopstmt.body, noerr);
		break;
	case Niterstmt:
		typesub(n->iterstmt.elt, noerr);
		typesub(n->iterstmt.seq, noerr);
		typesub(n->iterstmt.body, noerr);
		if (!ingeneric)
			additerspecialization(n, curstab());
		break;
	case Nmatchstmt:
		typesub(n->matchstmt.val, noerr);
		for (i = 0; i < n->matchstmt.nmatches; i++) {
			typesub(n->matchstmt.matches[i], noerr);
		}
		break;
	case Nmatch:
		typesub(n->match.pat, noerr);
		typesub(n->match.block, noerr);
		break;
	case Nexpr:
		settype(n, tyfix(n, type(n), 0));
		if (n->expr.param)
			n->expr.param = tyfix(n, n->expr.param, 0);
		typesub(n->expr.idx, noerr);
		if (exprop(n) == Ocast && exprop(n->expr.args[0]) == Olit) {
			l = n->expr.args[0]->expr.args[0];
			if(l->lit.littype == Lint && istyint(exprtype(n))) {
				settype(n->expr.args[0], exprtype(n));
				settype(n->expr.args[0]->expr.args[0], exprtype(n));
			}
		}
		if (exprop(n) == Oauto)
			adddispspecialization(n, curstab());
		for (i = 0; i < n->expr.nargs; i++)
			typesub(n->expr.args[i], noerr);
		if (!noerr)
			verifyop(n);
		break;
	case Nfunc:
		pushstab(n->func.scope);
		settype(n, tyfix(n, n->func.type, 0));
		for (i = 0; i < n->func.nargs; i++)
			typesub(n->func.args[i], noerr);
		typesub(n->func.body, noerr);
		popstab();
		break;
	case Nlit:
		settype(n, tyfix(n, type(n), 0));
		switch (n->lit.littype) {
		case Lfunc:	typesub(n->lit.fnval, noerr);	break;
		case Lint:	checkrange(n);			break;
		default: 					break;
		}
		break;
	case Nimpl:
		pushenv(n->impl.env);
		putimpl(curstab(), n);
		popenv(n->impl.env);
	case Nname: case Nuse:
		break;
	case Nnone:
		die("Nnone should not be seen as node type!");
		break;
	}
}

static Type *
itertype(Node *n, Type *ret)
{
	Type *it, *val, *itp, *valp, *fn;

	it = exprtype(n);
	itp = mktyptr(n->loc, it);
	val = basetype(it);
	if (!val)
		die("FAIL! %s", tystr(it));
	valp = mktyptr(n->loc, val);
	fn = mktyfunc(n->loc, NULL, 0, ret);
	lappend(&fn->sub, &fn->nsub, itp);
	lappend(&fn->sub, &fn->nsub, valp);
	return fn;
}

/* Take generics and build new versions of them
 * with the type parameters replaced with the
 * specialized types */
static void
specialize(void)
{
	Node *d, *n, *name;
	Type *ty, *it, *dt;
	size_t i;
	Trait *tr;

	for (i = 0; i < nimpldecl; i++) {
		d = impldecl[i];
		lappend(&file.stmts, &file.nstmts, d);
		typesub(d, 0);
	}

	for (i = 0; i < nspecializations; i++) {
		pushstab(specializationscope[i]);
		n = specializations[i];
		if (n->type == Nexpr && exprop(n) == Oauto) {
			tr = traittab[Tcdisp];
			assert(tr->nproto == 1);
			ty = exprtype(n);
			dt = mktyfunc(n->loc, NULL, 0, mktype(n->loc, Tyvoid));
			lappend(&dt->sub, &dt->nsub, ty);
			d = specializedcl(tr->proto[0], ty, dt, &name);
			htput(tr->proto[0]->decl.impls, ty, d);
		} else if (n->type == Nexpr && exprop(n) == Ovar) {
			d = specializedcl(genericdecls[i], n->expr.param, n->expr.type, &name);
			n->expr.args[0] = name;
			n->expr.did = d->decl.did;

			/* we need to sub in default types in the specialization, so call
			 * typesub on the specialized function */
			typesub(d, 0);
		} else if (n->type == Niterstmt) {
			tr = traittab[Tciter];
			assert(tr->nproto == 2);
			ty = exprtype(n->iterstmt.seq);
			if (ty->type == Typaram)
				goto enditer;

			it = itertype(n->iterstmt.seq, mktype(n->loc, Tybool));
			d = specializedcl(tr->proto[0], ty, it, &name);
			htput(tr->proto[0]->decl.impls, ty, d);

			it = itertype(n->iterstmt.seq, mktype(n->loc, Tyvoid));
			d = specializedcl(tr->proto[1], ty, it, &name);
			htput(tr->proto[1]->decl.impls, ty, d);
		} else {
			die("unknown node for specialization\n");
		}
enditer:
		popstab();
	}
}

static Traitmap *
mktraitmap(void)
{
	Traitmap *m;

	m = zalloc(sizeof(Traitmap));
	m->traits = mkbs();
	m->name = mkht(namehash, nameeq);
	return m;
}

static void
builtintraits(void)
{
	size_t i;

	/* char::(numeric,integral) */
	for (i = 0; i < Ntypes; i++)
		tytraits[i] = mkbs();

	bsput(tytraits[Tychar], Tcnum);
	bsput(tytraits[Tychar], Tcint);

	bsput(tytraits[Tybyte], Tcnum);
	bsput(tytraits[Tybyte], Tcint);

	/* <integer types>::(numeric,integral) */
	for (i = Tyint8; i < Tyflt32; i++) {
		bsput(tytraits[i], Tcnum);
		bsput(tytraits[i], Tcint);
	}

	/* <floats>::(numeric,floating) */
	bsput(tytraits[Tyflt32], Tcnum);
	bsput(tytraits[Tyflt32], Tcflt);
	bsput(tytraits[Tyflt64], Tcnum);
	bsput(tytraits[Tyflt64], Tcflt);

	/* @a*::(sliceable) */
	bsput(tytraits[Typtr], Tcslice);

	/* @a[:]::(indexable,sliceable) */
	bsput(tytraits[Tyslice], Tcidx);
	bsput(tytraits[Tyslice], Tcslice);
	bsput(tytraits[Tyslice], Tciter);

	/* @a[SZ]::(indexable,sliceable) */
	bsput(tytraits[Tyarray], Tcidx);
	bsput(tytraits[Tyarray], Tcslice);
	bsput(tytraits[Tyarray], Tciter);

	/* @a::function */
	bsput(tytraits[Tyfunc], Tcfunc);

	for (i = 0; i < Ntypes; i++) {
		traitmap->sub[i] = mktraitmap();
		bsunion(traitmap->sub[i]->traits, tytraits[i]);
	}

}

static Trait*
findtrait(Node *impl)
{
	Trait *tr;
	Node *n;
	Stab *ns;

	tr = impl->impl.trait;
	if (!tr) {
		n = impl->impl.traitname;
		ns = file.globls;
		if (n->name.ns)
			ns = getns(n->name.ns);
		if (ns)
			tr = gettrait(ns, n);
		if (!tr)
			fatal(impl, "trait %s does not exist near %s",
			    namestr(impl->impl.traitname), ctxstr(impl));
		if (tr->naux != impl->impl.naux)
			fatal(impl, "incompatible implementation of %s: mismatched aux types",
			    namestr(impl->impl.traitname), ctxstr(impl));
	}
	return tr;
}

static void
addtraittab(Traitmap *m, Trait *tr, Type *ty)
{
	Bitset *bs;
	Traitmap *mm;
	size_t i;

	if (!m->sub[ty->type])
		m->sub[ty->type] = mktraitmap();
	mm = m->sub[ty->type];
	switch (ty->type) {
	case Tygeneric:
	case Typaram:
		for (i = 0; i < Ntypes; i++) {
			if (i == Typaram)
				continue;
			if (!m->sub[i])
				m->sub[i] = mktraitmap();
			mm = m->sub[i];
			lappend(&mm->filter, &mm->nfilter, ty);
			lappend(&mm->filtertr, &mm->nfiltertr, tr);
		}
		break;
	case Tyname:
		if (ty->ngparam == 0) {
			bs = htget(mm->name, ty->name);
			if (!bs) {
				bs = mkbs();
				htput(mm->name, ty->name, bs);
			}
			bsput(bs, tr->uid);
		} else {
			lappend(&mm->filter, &m->nfilter, ty);
			lappend(&mm->filtertr, &m->nfiltertr, tr);
		}
		break;
	case Typtr:
	case Tyarray:
		addtraittab(mm, tr, ty->sub[0]);
		break;
	default:
		if (istyprimitive(ty)) {
			bsput(mm->traits, tr->uid);
		} else {
			lappend(&mm->filter, &mm->nfilter, ty);
			lappend(&mm->filtertr, &mm->nfiltertr, tr);
		}
	}
}

static void
initimpl(void)
{
	size_t i;
	Node *impl;
	Trait *tr;
	Type *ty;

	pushstab(file.globls);
	traitmap = zalloc(sizeof(Traitmap));
	builtintraits();
	for (i = 0; i < nimpltab; i++) {
		impl = impltab[i];
		tr = findtrait(impl);

		pushenv(impl->impl.env);
		ty = tf(impl->impl.type);
		addtraittab(traitmap, tr, ty);
		if (tr->uid == Tciter)
			ty->seqaux = tf(impl->impl.aux[0]);
		popenv(impl->impl.env);
	}
	popstab();
}

static void
verify(void)
{
	Type *t;
	Node *n;
	size_t i;

	pushstab(file.globls);
	/* for now, traits can only be declared globally */
	for (i = 0; i < file.nstmts; i++) {
		n = file.stmts[i];
		if (n->type == Nimpl) {
			/* we merge, so we need to get it back again when error checking */
			if (n->impl.isproto)
				fatal(n, "missing implementation for prototype '%s %s'",
					namestr(n->impl.traitname), tystr(n->impl.type));
		}
	}
	for (i = 0; i < ntypes; i++) {
		t = types[i];
		if (t->type != Tyarray)
			continue;
		t->asize = fold(t->asize, 1);
		if (t->asize && exprop(t->asize) != Olit)
			fatal(t->asize, "nonconstant array size near %s\n", ctxstr(t->asize));
	}
}

void
infer(void)
{
	size_t i;

	delayed = mkht(tyhash, tyeq);
	initimpl();

	/* do the inference */
	pushstab(file.globls);
	inferstab(file.globls);
	for (i = 0; i < file.nstmts; i++)
		infernode(&file.stmts[i], NULL, 0);
	popstab();
	postinfer();

	/* and replace type vars with actual types */
	pushstab(file.globls);
	stabsub(file.globls);
	for (i = 0; i < file.nstmts; i++)
		typesub(file.stmts[i], 0);
	popstab();

	specialize();
	verify();
}
