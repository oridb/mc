#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
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
size_t ntraittab;

/* Built in type constraints */
static Trait *traits[Ntypes + 1][4];
static int tybfmt(char *buf, size_t len, Type *t);

char stackness[] = {
#define Ty(t, n, stk) stk,
#include "types.def"
#undef Ty
};

int isstacktype(Type *t) { return stackness[tybase(t)->type]; }

Type *mktype(Srcloc loc, Ty ty)
{
	Type *t;
	int i;

	/* the first 'n' types will be identity mapped: tytab[Tyint], eg,
	 * will map to an instantitaion of Tyint.
	 *
	 * This is accomplished at program startup by calling mktype() on
	 * each builtin type in order. As we do this, we put the type into
	 * the table as ususal, which gives us an identity mapping.
	 */
	if (ty <= Tyvalist && ty < ntypes)
		return types[ty];

	t = zalloc(sizeof(Type));
	t->type = ty;
	t->tid = ntypes++;
	t->loc = loc;
	tytab = xrealloc(tytab, ntypes * sizeof(Type *));
	tytab[t->tid] = NULL;
	types = xrealloc(types, ntypes * sizeof(Type *));
	types[t->tid] = t;
	if (ty <= Tyvalist) /* the last builtin atomic type */
		t->vis = Visbuiltin;

	for (i = 0; traits[ty][i]; i++)
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

	r = mktype(t->loc, t->type);
	r->resolved = 0;	/* re-resolving doesn't hurt */
	r->fixed = 0;		/* re-resolving doesn't hurt */

	r->traits = bsdup(t->traits);
	r->traitlist = memdup(t->traitlist, t->ntraitlist * sizeof(Node *));
	r->ntraitlist = t->ntraitlist;

	r->arg = memdup(t->arg, t->narg * sizeof(Type *));
	r->narg = t->narg;
	r->inst = memdup(t->arg, t->narg * sizeof(Type *));
	r->ninst = t->ninst;

	r->sub = memdup(t->sub, t->nsub * sizeof(Type *));
	r->nsub = t->nsub;
	r->nmemb = t->nmemb;
	switch (t->type) {
	case Tyname:	r->name = t->name;	break;
	case Tyunres:	r->name = t->name;	break;
	case Tyarray:	r->asize = t->asize;	break;
	case Typaram:	r->pname = strdup(t->pname);	break;
	case Tystruct:	r->sdecls = memdup(t->sdecls, t->nmemb * sizeof(Node *));	break;
	case Tyunion:	r->udecls = memdup(t->udecls, t->nmemb * sizeof(Node *));	break;
	default: break;
	}
	return r;
}

/*
 * Creates a Tyvar with the same
 * constrants as the 'like' type
 */
Type *mktylike(Srcloc loc, Ty like)
{
	Type *t;
	int i;

	t = mktyvar(loc);
	for (i = 0; traits[like][i]; i++)
		settrait(t, traits[like][i]);
	return t;
}

/* steals memb, funcs */
Trait *mktrait(Srcloc loc, Node *name, Type *param, Node **memb, size_t nmemb, Node **funcs,
		size_t nfuncs, int isproto)
{
	Trait *t;

	t = zalloc(sizeof(Trait));
	t->uid = ntraittab++;
	t->loc = loc;
	if (t->uid < Ntraits)
		t->vis = Visbuiltin;
	t->vis = Visintern;
	t->name = name;
	t->param = param;
	t->memb = memb;
	t->nmemb = nmemb;
	t->funcs = funcs;
	t->nfuncs = nfuncs;
	t->isproto = isproto;

	traittab = xrealloc(traittab, ntraittab * sizeof(Trait *));
	traittab[t->uid] = t;
	return t;
}

Type *mktyvar(Srcloc loc)
{
	Type *t;

	t = mktype(loc, Tyvar);
	return t;
}

Type *mktyparam(Srcloc loc, char *name)
{
	Type *t;

	t = mktype(loc, Typaram);
	t->pname = strdup(name);
	return t;
}

Type *mktyunres(Srcloc loc, Node *name, Type **arg, size_t narg)
{
	Type *t;

	/* resolve it in the type inference stage */
	t = mktype(loc, Tyunres);
	t->name = name;
	t->arg = arg;
	t->narg = narg;
	return t;
}

Type *mktygeneric(Srcloc loc, Node *name, Type **param, size_t nparam, Type *base)
{
	Type *t;

	t = mktype(loc, Tygeneric);
	t->name = name;
	t->nsub = 1;
	t->traits = bsdup(base->traits);
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	t->gparam = param;
	t->ngparam = nparam;
	return t;
}

Type *mktyname(Srcloc loc, Node *name, Type *base)
{
	Type *t;

	t = mktype(loc, Tyname);
	t->name = name;
	t->nsub = 1;
	t->traits = bsdup(base->traits);
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	return t;
}

Type *mktyarray(Srcloc loc, Type *base, Node *sz)
{
	Type *t;

	t = mktype(loc, Tyarray);
	t->nsub = 1;
	t->nmemb = 1; /* the size is a "member" */
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	t->asize = sz;

	return t;
}

Type *mktyslice(Srcloc loc, Type *base)
{
	Type *t;

	t = mktype(loc, Tyslice);
	t->nsub = 1;
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	return t;
}

Type *mktyidxhack(Srcloc loc, Type *base)
{
	Type *t;

	t = mktype(loc, Tyvar);
	t->nsub = 1;
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	return t;
}

Type *mktyptr(Srcloc loc, Type *base)
{
	Type *t;

	t = mktype(loc, Typtr);
	t->nsub = 1;
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	return t;
}

Type *mktytuple(Srcloc loc, Type **sub, size_t nsub)
{
	Type *t;
	size_t i;

	t = mktype(loc, Tytuple);
	t->nsub = nsub;
	t->sub = xalloc(nsub * sizeof(Type *));
	for (i = 0; i < nsub; i++)
		t->sub[i] = sub[i];
	return t;
}

Type *mktyfunc(Srcloc loc, Node **args, size_t nargs, Type *ret)
{
	Type *t;
	size_t i;

	t = mktype(loc, Tyfunc);
	t->nsub = nargs + 1;
	t->sub = xalloc((1 + nargs) * sizeof(Type *));
	t->sub[0] = ret;
	for (i = 0; i < nargs; i++)
		t->sub[i + 1] = nodetype(args[i]);
	return t;
}

Type *mktystruct(Srcloc loc, Node **decls, size_t ndecls)
{
	Type *t;

	t = mktype(loc, Tystruct);
	t->nsub = 0;
	t->nmemb = ndecls;
	t->sdecls = memdup(decls, ndecls * sizeof(Node *));
	return t;
}

Type *mktyunion(Srcloc loc, Ucon **decls, size_t ndecls)
{
	Type *t;

	t = mktype(loc, Tyunion);
	t->nmemb = ndecls;
	t->udecls = decls;
	return t;
}

Ucon *finducon(Type *ty, Node *name)
{
	size_t i;

	ty = tybase(ty);
	for (i = 0; i < ty->nmemb; i++)
		if (!strcmp(namestr(ty->udecls[i]->name), namestr(name)))
			return ty->udecls[i];
	return NULL;
}

int istyunsigned(Type *t)
{
	switch (tybase(t)->type) {
	case Tybyte:
	case Tyuint8:
	case Tyuint16:
	case Tyuint:
	case Tychar:
	case Tyuint32:
	case Tyuint64:
	case Typtr:
	case Tybool: return 1;
	default: return 0;
	}
}

int istysigned(Type *t)
{
	switch (tybase(t)->type) {
	case Tyint8:
	case Tyint16:
	case Tyint:
	case Tyint32:
	case Tyint64: return 1;
	default: return 0;
	}
}

int istyfloat(Type *t)
{
	t = tybase(t);
	return t->type == Tyflt32 || t->type == Tyflt64;
}

int istyprimitive(Type *t) { return istysigned(t) || istyunsigned(t) || istyfloat(t); }

/*
 * Checks if a type contains any type
 * parameers at all (ie, if it generic).
 */
int hasparamsrec(Type *t, Bitset *visited)
{
	size_t i;

	if (bshas(visited, t->tid))
		return 0;
	bsput(visited, t->tid);
	switch (t->type) {
	case Typaram:
	case Tygeneric: return 1;
	case Tyname:
			for (i = 0; i < t->narg; i++)
				if (hasparamsrec(t->arg[i], visited))
					return 1;
			return hasparamsrec(t->sub[0], visited);
	case Tyunres:
			for (i = 0; i < t->narg; i++)
				if (hasparamsrec(t->arg[i], visited))
					return 1;
			break;
	case Tystruct:
			for (i = 0; i < t->nmemb; i++)
				if (hasparamsrec(t->sdecls[i]->decl.type, visited))
					return 1;
			break;
	case Tyunion:
			for (i = 0; i < t->nmemb; i++)
				if (t->udecls[i]->etype && hasparamsrec(t->udecls[i]->etype, visited))
					return 1;
			break;
	default:
			for (i = 0; i < t->nsub; i++)
				if (hasparamsrec(t->sub[i], visited))
					return 1;
			break;
	}
	return 0;
}

int hasparams(Type *t)
{
	Bitset *visited;
	int r;

	visited = mkbs();
	r = hasparamsrec(t, visited);
	bsfree(visited);
	return r;
}

Type *tybase(Type *t)
{
	assert(t != NULL);
	while (t->type == Tyname || t->type == Tygeneric)
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
		p += bprintf(p, end - p, "%s.", n->name.ns);
	p += bprintf(p, end - p, "%s", n->name.name);
	return len - (end - p);
}

int settrait(Type *t, Trait *c)
{
	if (!t->traits)
		t->traits = mkbs();
	bsput(t->traits, c->uid);
	return 1;
}

int hastrait(Type *t, Trait *c) { return t->traits && bshas(t->traits, c->uid); }

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

	p += bprintf(p, end - p, " :: ");
	sep = "";
	for (i = 0; i < ntraittab; i++) {
		if (bshas(t->traits, i)) {
			p += bprintf(p, end - p, "%s%s", sep, namestr(traittab[i]->name));
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
	p += bprintf(p, end - p, "struct\n");
	for (i = 0; i < t->nmemb; i++) {
		name = declname(t->sdecls[i]);
		ty = tystr(decltype(t->sdecls[i]));
		p += bprintf(p, end - p, "\t%s:%s\n ", name, ty);
		free(ty);
	}
	p += bprintf(p, end - p, ";;");
	return p - buf;
}

static int fmtunion(char *buf, size_t len, Type *t)
{
	size_t i;
	char *end, *p;
	char *name, *ty;

	p = buf;
	end = p + len;
	p += bprintf(p, end - p, "union\n");
	for (i = 0; i < t->nmemb; i++) {
		name = namestr(t->udecls[i]->name);
		if (t->udecls[i]->etype) {
			ty = tystr(t->udecls[i]->etype);
			p += bprintf(p, end - p, "\t`%s %s\n", name, ty);
			free(ty);
		}
		else {
			p += bprintf(p, end - p, "\t`%s\n", name);
		}
	}
	p += bprintf(p, end - p, ";;");
	return p - buf;
}

static int fmtlist(char *buf, size_t len, Type **arg, size_t narg)
{
	char *end, *p, *sep;
	size_t i;

	sep = "";
	p = buf;
	end = p + len;
	p += bprintf(p, end - p, "(");
	for (i = 0; i < narg; i++) {
		p += bprintf(p, end - p, "%s", sep);
		p += tybfmt(p, end - p, arg[i]);
		sep = ", ";
	}
	p += bprintf(p, end - p, ")");
	return p - buf;
}

static int tybfmt(char *buf, size_t len, Type *t)
{
	size_t i;
	char *p;
	char *end;
	char *sep;

	sep = "";
	p = buf;
	end = p + len;
	if (!t) {
		p += bprintf(p, end - p, "tynil");
		return len - (end - p);
	}
	switch (t->type) {
	case Tybad:	p += bprintf(p, end - p, "BAD");	break;
	case Tyvoid:	p += bprintf(p, end - p, "void");	break;
	case Tybool:	p += bprintf(p, end - p, "bool");	break;
	case Tychar:	p += bprintf(p, end - p, "char");	break;
	case Tyint8:	p += bprintf(p, end - p, "int8");	break;
	case Tyint16:	p += bprintf(p, end - p, "int16");	break;
	case Tyint:	p += bprintf(p, end - p, "int");	break;
	case Tyint32:	p += bprintf(p, end - p, "int32");	break;
	case Tyint64:	p += bprintf(p, end - p, "int64");	break;
	case Tybyte:	p += bprintf(p, end - p, "byte");	break;
	case Tyuint8:	p += bprintf(p, end - p, "uint8");	break;
	case Tyuint16:	p += bprintf(p, end - p, "uint16");	break;
	case Tyuint:	p += bprintf(p, end - p, "uint");	break;
	case Tyuint32:	p += bprintf(p, end - p, "uint32");	break;
	case Tyuint64:	p += bprintf(p, end - p, "uint64");	break;
	case Tyflt32:	p += bprintf(p, end - p, "flt32");	break;
	case Tyflt64:	p += bprintf(p, end - p, "flt64");	break;
	case Tyvalist:	p += bprintf(p, end - p, "...");	break;

	case Typtr:
		p += tybfmt(p, end - p, t->sub[0]);
		p += bprintf(p, end - p, "#");
		break;
	case Tyslice:
		p += tybfmt(p, end - p, t->sub[0]);
		p += bprintf(p, end - p, "[:]");
		break;
	case Tyarray:
		p += tybfmt(p, end - p, t->sub[0]);
		if (t->asize) {
			i = t->asize->expr.args[0]->lit.intval;
			p += bprintf(p, end - p, "[%zd]", i);
		}
		else {
			p += bprintf(p, end - p, "[]");
		}
		break;
	case Tycode:
	case Tyfunc:
		p += bprintf(p, end - p, "(");
		for (i = 1; i < t->nsub; i++) {
			p += bprintf(p, end - p, "%s", sep);
			p += tybfmt(p, end - p, t->sub[i]);
			sep = ", ";
		}
		p += bprintf(p, end - p, " -> ");
		p += tybfmt(p, end - p, t->sub[0]);
		p += bprintf(p, end - p, ")");
		break;
	case Tytuple:
		p += bprintf(p, end - p, "(");
		for (i = 0; i < t->nsub; i++) {
			p += bprintf(p, end - p, "%s", sep);
			p += tybfmt(p, end - p, t->sub[i]);
			sep = ",";
		}
		p += bprintf(p, end - p, ")");
		break;
	case Tyvar:
		p += bprintf(p, end - p, "$%d", t->tid);
		if (t->nsub) {
			p += bprintf(p, end - p, "(");
			for (i = 0; i < t->nsub; i++) {
				p += bprintf(p, end - p, "%s", sep);
				p += tybfmt(p, end - p, t->sub[i]);
				sep = ", ";
			}
			p += bprintf(p, end - p, ")[]");
		}
		break;
	case Typaram:	p += bprintf(p, end - p, "@%s", t->pname);	break;
	case Tyunres:
		p += namefmt(p, end - p, t->name);
		if (t->narg)
			p += fmtlist(p, end - p, t->arg, t->narg);
		break;
	case Tyname:
		if (t->name->name.ns)
			p += bprintf(p, end - p, "%s.", t->name->name.ns);
		p += bprintf(p, end - p, "%s", namestr(t->name));
		if (t->narg)
			p += fmtlist(p, end - p, t->arg, t->narg);
		break;
	case Tygeneric:
		if (t->name->name.ns)
			p += bprintf(p, end - p, "%s.", t->name->name.ns);
		p += bprintf(p, end - p, "%s", namestr(t->name));
		if (t->ngparam)
			p += fmtlist(p, end - p, t->gparam, t->ngparam);
		break;
	case Tystruct:	p += fmtstruct(p, end - p, t);	break;
	case Tyunion:	p += fmtunion(p, end - p, t);	break;
	case Ntypes:	die("Ntypes is not a type");	break;
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
	switch (t->type) {
		/* Important: we want tyhash to be consistent cross-file, since it
		 * is used in naming trait impls and such.
		 *
		 * We should find a better name.
		 */
	case Tyvar:	hash = inthash(t->tid);	break;
	case Typaram:	hash = strhash(t->pname);	break;
	case Tyunion:	hash = inthash(t->type);	break;
	case Tystruct:	hash = inthash(t->type);	break;
	case Tyname:	hash = namehash(t->name);	break;
	default: hash = inthash(t->type); break;
	}

	for (i = 0; i < t->narg; i++)
		hash ^= tyhash(t->arg[i]);
	return hash;
}

int tyeq_rec(Type *a, Type *b, Bitset *visited)
{
	size_t i;

	if (!a || !b)
		return a == b;
	if (a->type != b->type)
		return 0;
	if (a->narg != b->narg)
		return 0;
	if (a->nsub != b->nsub)
		return 0;
	if (a->nmemb != b->nmemb)
		return 0;

	if (a->tid == b->tid)
		return 1;
	if (bshas(visited, a->tid) || bshas(visited, b->tid))
		return 1;

	bsput(visited, a->tid);
	bsput(visited, b->tid);

	switch (a->type) {
	case Typaram:	return streq(a->pname, b->pname);	break;
	case Tyvar:
		if (a->tid != b->tid)
			return 0;
		break;
	case Tyunres:
		if (!nameeq(a->name, b->name))
			return 0;
	case Tyunion:
		for (i = 0; i < a->nmemb; i++) {
			if (!nameeq(a->udecls[i]->name, b->udecls[i]->name))
				return 0;
			if (!tyeq_rec(a->udecls[i]->etype, b->udecls[i]->etype, visited))
				return 0;
		}
		break;
	case Tystruct:
		for (i = 0; i < a->nmemb; i++) {
			if (strcmp(declname(a->sdecls[i]), declname(b->sdecls[i])) != 0)
				return 0;
			if (!tyeq_rec(decltype(a->sdecls[i]), decltype(b->sdecls[i]), visited))
				return 0;
		}
		break;
	case Tyname:
		if (!nameeq(a->name, b->name))
			return 0;
		for (i = 0; i < a->narg; i++)
			if (!tyeq_rec(a->arg[i], b->arg[i], visited))
				return 0;
		for (i = 0; i < a->nsub; i++)
			if (!tyeq_rec(a->sub[i], b->sub[i], visited))
				return 0;
		break;
	case Tyarray:
		if (arraysz(a->asize) != arraysz(b->asize))
			return 0;
		break;
	default: break;
	}
	for (i = 0; i < a->nsub; i++)
		if (!tyeq_rec(a->sub[i], b->sub[i], visited))
			return 0;
	return 1;
}

int tyeq(void *a, void *b)
{
	Bitset *bs;
	int eq;

	if (a == b)
		return 1;
	bs = mkbs();
	eq = tyeq_rec(a, b, bs);
	bsfree(bs);
	return eq;
}

size_t tyidfmt(char *buf, size_t sz, Type *ty)
{
	size_t i;
	char *p, *end;

	p = buf;
	end = buf + sz;
	switch (ty->type) {
	case Ntypes:
	case Tybad:	die("invalid type");	break;
	case Tyvar:	die("tyvar has no idstr");	break;
	case Tyvoid:	p += bprintf(p, end - p, "v");	break;
	case Tychar:	p += bprintf(p, end - p, "c");	break;
	case Tybool:	p += bprintf(p, end - p, "t");	break;
	case Tyint8:	p += bprintf(p, end - p, "b");	break;
	case Tyint16:	p += bprintf(p, end - p, "s");	break;
	case Tyint:	p += bprintf(p, end - p, "i");	break;
	case Tyint32:	p += bprintf(p, end - p, "w");	break;
	case Tyint64:	p += bprintf(p, end - p, "q");	break;

	case Tybyte:	p += bprintf(p, end - p, "H");	break;
	case Tyuint8:	p += bprintf(p, end - p, "B");	break;
	case Tyuint16:	p += bprintf(p, end - p, "S");	break;
	case Tyuint:	p += bprintf(p, end - p, "I");	break;
	case Tyuint32:	p += bprintf(p, end - p, "W");	break;
	case Tyuint64:	p += bprintf(p, end - p, "Q");	break;
	case Tyflt32:	p += bprintf(p, end - p, "f");	break;
	case Tyflt64:	p += bprintf(p, end - p, "d");	break;
	case Tyvalist:	p += bprintf(p, end - p, "V");	break;
	case Typtr:
		p += bprintf(p, end - p, "$p");
		p += tyidfmt(p, end - p, ty->sub[0]);
		break;
	case Tyarray:
		p += bprintf(p, end - p, "$a%lld", (vlong)arraysz(ty->asize));
		p += tyidfmt(p, end - p, ty->sub[0]);
		break;
	case Tyslice:
		p += bprintf(p, end - p, "$s");
		p += tyidfmt(p, end - p, ty->sub[0]);
		break;
	case Tycode:
		p += bprintf(p, end - p, "$F");
		for (i = 0; i < ty->nsub; i++) {
			p += tyidfmt(p, end - p, ty->sub[i]);
			p += bprintf(p, end - p, "$");
		}
		break;
	case Tyfunc:
		p += bprintf(p, end - p, "$f");
		for (i = 0; i < ty->nsub; i++) {
			p += tyidfmt(p, end - p, ty->sub[i]);
			p += bprintf(p, end - p, "$");
		}
		break;
	case Tytuple:
		p += bprintf(p, end - p, "$e");
		for (i = 0; i < ty->nsub; i++) {
			p += tyidfmt(p, end - p, ty->sub[i]);
		}
		p += bprintf(p, end - p, "$");
		break;
	case Tystruct:	p += bprintf(p, end - p, "$t%lld", ty->tid);	break;
	case Tyunion:	p += bprintf(p, end - p, "$u%lld", ty->tid);	break;
	case Typaram:	p += bprintf(p, end - p, "$r%s", ty->pname);	break;
	case Tyunres:
	case Tyname:
		p += bprintf(p, end - p, "$n");
		if (ty->name->name.ns)
			p += bprintf(p, end - p, "%s", ty->name->name.ns);
		p += bprintf(p, end - p, "$%s", ty->name->name.name);
		if (ty->arg)
			for (i = 0; i < ty->narg; i++)
				p += tyidfmt(p, end - p, ty->arg[i]);
		else if (ty->gparam)
			for (i = 0; i < ty->ngparam; i++)
				p += tyidfmt(p, end - p, ty->gparam[i]);
		break;
	case Tygeneric:
		break;
	}
	return p - buf;
}

void tyinit(Stab *st)
{
	int i;
	Type *ty;

	/* this must be done after all the types are created, otherwise we will
	 * clobber the memoized bunch of types with the type params. */
#define Tc(c, n) mktrait(Zloc, mkname(Zloc, n), NULL, NULL, 0, NULL, 0, 0);
#include "trait.def"
#undef Tc

	/* char::(numeric,integral) */
	traits[Tychar][0] = traittab[Tcnum];
	traits[Tychar][1] = traittab[Tcint];

	traits[Tybyte][0] = traittab[Tcnum];
	traits[Tybyte][1] = traittab[Tcint];

	/* <integer types>::(numeric,integral) */
	for (i = Tyint8; i < Tyflt32; i++) {
		traits[i][0] = traittab[Tcnum];
		traits[i][1] = traittab[Tcint];
	}

	/* <floats>::(numeric,floating) */
	traits[Tyflt32][0] = traittab[Tcnum];
	traits[Tyflt32][1] = traittab[Tcfloat];
	traits[Tyflt64][0] = traittab[Tcnum];
	traits[Tyflt64][1] = traittab[Tcfloat];

	/* @a*::(sliceable) */
	traits[Typtr][0] = traittab[Tcslice];

	/* @a[:]::(indexable,sliceable) */
	traits[Tyslice][0] = traittab[Tcslice];
	traits[Tyslice][1] = traittab[Tcidx];

	/* @a[SZ]::(indexable,sliceable) */
	traits[Tyarray][0] = traittab[Tcidx];
	traits[Tyarray][1] = traittab[Tcslice];

	/* @a::function */
	traits[Tyfunc][0] = traittab[Tcfunc];

	/* Definining and registering the types has to go after we define the
	 * constraints, otherwise they will have no constraints set on them. */
#define Ty(t, n, stk)		\
	if (t != Ntypes) {	\
		ty = mktype(Zloc, t);	\
		if (n) {	\
			puttype(st, mkname(Zloc, n), ty);	\
		}	\
	}
#include "types.def"
#undef Ty
}
