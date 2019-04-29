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

#include "util.h"
#include "parse.h"

typedef struct Typename Typename;
typedef struct Typair Typair;
Type **tytab = NULL;
Type **types = NULL;
size_t ntypes;
Trait **traittab;
size_t ntraittab;
Node **impltab;
size_t nimpltab;
Htab *eqcache;

struct Typair {
	uint32_t atid;
	uint32_t btid;
};
	

static Htab *tydeduptab;
/* Built in type constraints */
static int tybfmt(char *buf, size_t len, Bitset *visted, Type *t);

char stackness[] = {
#define Ty(t, n, stk) stk,
#include "types.def"
#undef Ty
};

int
isstacktype(Type *t)
{
	t = tybase(t);
	if (t->type == Tyunion)
		return !isenum(t);
	return stackness[t->type];
}

int
isenum(Type *t)
{
	size_t i;
	char isenum;

	assert(t->type == Tyunion);

	/* t->isenum is lazily set:
	 * a value of 0 means that it was not computed,
	 * a value of 1 means that the type is an enum
	 * (i.e., it only has nullary constructors)
	 * a value of 2 means that the type is not an enum
	 */
	if (t->isenum == 0) {
		/* initialize it */
		isenum = 1;
		for (i = 0; i < t->nmemb; i++)
			if (t->udecls[i]->etype) {
				isenum = 2;
				break;
			}
		t->isenum = isenum;
	}
	return t->isenum == 1;
}

Type *
tydedup(Type *ty)
{
	Type *had;

	had = htget(tydeduptab, ty);
	if (!had) {
		htput(tydeduptab, ty, ty);
		return ty;
	}
	/* if one is emitted, both are */
	ty->isemitted = ty->isemitted || had->isemitted;
	return had;
}

Type *
mktype(Srcloc loc, Ty ty)
{
	Type *t;

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

	return t;
}

/*
 * Shallowly duplicates a type, so we can frob
 * its internals later
 */
Type *
tydup(Type *t)
{
	Type *r;

	r = mktype(t->loc, t->type);
	r->resolved = 0;	/* re-resolving doesn't hurt */
	r->fixed = 0;		/* re-resolving doesn't hurt */

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

/* steals memb, funcs */
Trait *
mktrait(Srcloc loc, Node *name, Type *param,
	Type **aux, size_t naux,
	Node **proto, size_t nproto,
	int isproto)
{
	Trait *t;
	size_t i;

	t = zalloc(sizeof(Trait));
	t->uid = ntraittab++;
	t->loc = loc;
	if (t->uid < Ntraits)
		t->vis = Visbuiltin;
	t->vis = Visintern;
	t->name = name;
	t->param = param;
	t->proto = proto;
	t->nproto = nproto;
	t->aux = aux;
	t->env = mkenv();
	t->naux = naux;
	t->isproto = isproto;

	bindtype(t->env, param);
	for (i = 0; i < naux; i++)
		bindtype(t->env, aux[i]);

	for (i = 0; i < nproto; i++)
		if (proto[i]->decl.env)
			proto[i]->decl.env->super = t->env;


	traittab = xrealloc(traittab, ntraittab * sizeof(Trait *));
	traittab[t->uid] = t;
	return t;
}

Type *
mktyvar(Srcloc loc)
{
	Type *t;

	t = mktype(loc, Tyvar);
	return t;
}

Type *
mktyparam(Srcloc loc, char *name)
{
	Type *t;

	t = mktype(loc, Typaram);
	t->pname = strdup(name);
	return t;
}

Type *
mktyunres(Srcloc loc, Node *name, Type **arg, size_t narg)
{
	size_t i;
	Type *t;

	/* resolve it in the type inference stage */
	t = mktype(loc, Tyunres);
	t->name = name;
	t->arg = arg;
	t->narg = narg;
	t->env = mkenv();
	for (i = 0; i < narg; i++)
		bindtype(t->env, arg[i]);
	return t;
}

Type *
mktygeneric(Srcloc loc, Node *name, Type **param, size_t nparam, Type *base)
{
	Type *t;
	int i;

	t = mktype(loc, Tygeneric);
	t->name = name;
	t->nsub = 1;
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	t->gparam = param;
	t->ngparam = nparam;
	t->env = mkenv();
	for (i = 0; i < nparam; i++)
		bindtype(t->env, param[i]);
	if (!base->env)
		base->env = t->env;
	return t;
}

Type *
mktyname(Srcloc loc, Node *name, Type *base)
{
	Type *t;

	t = mktype(loc, Tyname);
	t->name = name;
	t->nsub = 1;
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	return t;
}

Type *
mktyarray(Srcloc loc, Type *base, Node *sz)
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

Type *
mktyslice(Srcloc loc, Type *base)
{
	Type *t;

	t = mktype(loc, Tyslice);
	t->nsub = 1;
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	return t;
}

Type *
mktyptr(Srcloc loc, Type *base)
{
	Type *t;

	t = mktype(loc, Typtr);
	t->nsub = 1;
	t->sub = xalloc(sizeof(Type *));
	t->sub[0] = base;
	return t;
}

Type *
mktytuple(Srcloc loc, Type **sub, size_t nsub)
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

Type *
mktyfunc(Srcloc loc, Node **args, size_t nargs, Type *ret)
{
	Type *t;
	size_t i;

	t = mktype(loc, Tyfunc);
	t->nsub = nargs + 1;
	t->sub = xalloc((1 + nargs) * sizeof(Type *));
	t->sub[0] = ret;
	t->env = mkenv();
	for (i = 0; i < nargs; i++)
		t->sub[i + 1] = nodetype(args[i]);
	for (i = 0; i < t->nsub; i++)
		bindtype(t->env, t->sub[i]);
	return t;
}

Type *
mktystruct(Srcloc loc, Node **decls, size_t ndecls)
{
	Type *t;
	Htab *ht;
	int i;
	char *s;

	t = mktype(loc, Tystruct);
	t->nsub = 0;
	t->nmemb = ndecls;

	ht = mkht(strhash, streq);
	for (i = 0; i < ndecls; i++) {
		s = declname(decls[i]);
		if (hthas(ht, s))
			fatal(decls[i], "duplicate struct member %s\n", declname(decls[i]));
		htput(ht, s, s);
	}
	htfree(ht);

	t->sdecls = memdup(decls, ndecls * sizeof(Node *));
	return t;
}

Type *
mktyunion(Srcloc loc, Ucon **decls, size_t ndecls)
{
	Type *t;

	t = mktype(loc, Tyunion);
	t->nmemb = ndecls;
	t->udecls = decls;
	return t;
}

Ucon *
finducon(Type *ty, Node *name)
{
	size_t i;

	ty = tybase(ty);
	for (i = 0; i < ty->nmemb; i++)
		if (!strcmp(namestr(ty->udecls[i]->name), namestr(name)))
			return ty->udecls[i];
	return NULL;
}

int
istyunsigned(Type *t)
{
	switch (tybase(t)->type) {
	case Tybyte:
	case Tyuint8:
	case Tyuint16:
	case Tyuint:
	case Tychar:
	case Tyuint32:
	case Tyuint64:
	case Tybool: return 1;
	default: return 0;
	}
}

int
istysigned(Type *t)
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

int
istyint(Type *t)
{
	return istysigned(t) || istyunsigned(t);
}


int
istyfloat(Type *t)
{
	t = tybase(t);
	return t->type == Tyflt32 || t->type == Tyflt64;
}

int
istyprimitive(Type *t) { return istysigned(t) || istyunsigned(t) || istyfloat(t); }

/*
 * Checks if a type contains any type
 * parameers at all (ie, if it generic).
 */
int
hasparamsrec(Type *t, Bitset *visited)
{
	size_t i;

	if (bshas(visited, t->tid))
		return 0;
	bsput(visited, t->tid);
	switch (t->type) {
	case Typaram:	return 1;
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

int
hasparams(Type *t)
{
	Bitset *visited;

	if (t->hasparams)
		return 1;
	visited = mkbs();
	t->hasparams = hasparamsrec(t, visited);
	bsfree(visited);
	return t->hasparams;
}

Type *
tybase(Type *t)
{
	assert(t != NULL);
	while (t->type == Tyname || t->type == Tygeneric)
		t = t->sub[0];
	return t;
}

static int
namefmt(char *buf, size_t len, Node *n)
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

int
traitfmt(char *buf, size_t len, Type *t)
{
	size_t i;
	char *p;
	char *end;
	char *sep;

	if (!t->trneed || !bscount(t->trneed))
		return 0;

	p = buf;
	end = p + len;

	p += bprintf(p, end - p, " :: ");
	sep = "";
	for (i = 0; bsiter(t->trneed, &i); i++) {
		p += bprintf(p, end - p, "%s%s", sep, namestr(traittab[i]->name));
		sep = ",";
	}
	return p - buf;
}

static int
fmtstruct(char *buf, size_t len, Bitset *visited, Type *t)
{
	size_t i;
	char *end, *p;
	char *name;
	char subbuf[512];

	p = buf;
	end = p + len;
	p += bprintf(p, end - p, "struct\n");
	for (i = 0; i < t->nmemb; i++) {
		name = declname(t->sdecls[i]);
		tybfmt(subbuf, sizeof subbuf, visited, decltype(t->sdecls[i]));
		p += bprintf(p, end - p, "\t%s:%s\n ", name, subbuf);
	}
	p += bprintf(p, end - p, ";;");
	return p - buf;
}

static int
fmtunion(char *buf, size_t len, Bitset *visited, Type *t)
{
	char subbuf[512];
	char *name, *ns, *sep;
	char *end, *p;
	size_t i;
	Node *n;

	p = buf;
	end = p + len;
	p += bprintf(p, end - p, "union\n");
	for (i = 0; i < t->nmemb; i++) {
		n = t->udecls[i]->name;
		if (n->name.ns) {
			sep = ".";
			ns = n->name.ns;
		} else {
			sep = "";
			ns = "";
		}
		name = n->name.name;
		if (t->udecls[i]->etype) {
			tybfmt(subbuf, sizeof subbuf, visited, t->udecls[i]->etype);
			p += bprintf(p, end - p, "\t`%s%s%s %s\n", ns, sep, name, subbuf);
		} else {
			p += bprintf(p, end - p, "\t`%s%s%s\n", ns, sep, name);
		}
	}
	p += bprintf(p, end - p, ";;");
	return p - buf;
}

static int
fmtlist(char *buf, size_t len, Bitset *visited, Type **arg, size_t narg)
{
	char *end, *p, *sep;
	size_t i;

	sep = "";
	p = buf;
	end = p + len;
	p += bprintf(p, end - p, "(");
	for (i = 0; i < narg; i++) {
		p += bprintf(p, end - p, "%s", sep);
		p += tybfmt(p, end - p, visited, arg[i]);
		sep = ", ";
	}
	p += bprintf(p, end - p, ")");
	return p - buf;
}

static int
tybfmt(char *buf, size_t len, Bitset *visited, Type *t)
{
	size_t i;
	char *p;
	char *end;
	char *sep;

	if (t)
		t = tysearch(t);

	sep = "";
	p = buf;
	end = p + len;

	if (!t) {
		p += bprintf(p, end - p, "tynil");
		return len - (end - p);
	}

	if (bshas(visited, t->tid)) {
		p += bprintf(buf, len, "<...>");
		return len - (end - p);
	}
        bsput(visited, t->tid);
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
	case Tyvar:     p += bprintf(p, end - p, "$%d", t->tid);        break;

	case Typtr:
		p += tybfmt(p, end - p, visited, t->sub[0]);
		p += bprintf(p, end - p, "#");
		break;
	case Tyslice:
		p += tybfmt(p, end - p, visited, t->sub[0]);
		p += bprintf(p, end - p, "[:]");
		break;
	case Tyarray:
		p += tybfmt(p, end - p, visited, t->sub[0]);
		if (t->asize) {
			i = t->asize->expr.args[0]->lit.intval;
			p += bprintf(p, end - p, "[%zd]", i);
		} else {
			p += bprintf(p, end - p, "[]");
		}
		break;
	case Tycode:
	case Tyfunc:
		p += bprintf(p, end - p, "(");
		for (i = 1; i < t->nsub; i++) {
			p += bprintf(p, end - p, "%s", sep);
			p += tybfmt(p, end - p, visited, t->sub[i]);
			sep = ", ";
		}
		p += bprintf(p, end - p, " -> ");
		p += tybfmt(p, end - p, visited, t->sub[0]);
		p += bprintf(p, end - p, ")");
		break;
	case Tytuple:
		p += bprintf(p, end - p, "(");
		for (i = 0; i < t->nsub; i++) {
			p += bprintf(p, end - p, "%s", sep);
			p += tybfmt(p, end - p, visited, t->sub[i]);
			sep = ",";
		}
		p += bprintf(p, end - p, ")");
		break;
	case Typaram:	p += bprintf(p, end - p, "@%s", t->pname);	break;
	case Tyunres:
		p += namefmt(p, end - p, t->name);
		if (t->narg)
			p += fmtlist(p, end - p, visited, t->arg, t->narg);
		break;
	case Tyname:
		if (t->name->name.ns)
			p += bprintf(p, end - p, "%s.", t->name->name.ns);
		p += bprintf(p, end - p, "%s", namestr(t->name));
		if (t->narg)
			p += fmtlist(p, end - p, visited, t->arg, t->narg);
		break;
	case Tygeneric:
		if (t->name->name.ns)
			p += bprintf(p, end - p, "%s.", t->name->name.ns);
		p += bprintf(p, end - p, "%s", namestr(t->name));
		if (t->ngparam)
			p += fmtlist(p, end - p, visited, t->gparam, t->ngparam);
		break;
	case Tystruct:	p += fmtstruct(p, end - p, visited, t);	break;
	case Tyunion:	p += fmtunion(p, end - p, visited, t);	break;
	case Ntypes:	die("Ntypes is not a type");	break;
	}

	/* we only show constraints on non-builtin typaram */
	if (t->type == Tyvar || t->type == Typaram)
		p += traitfmt(p, end - p, t);

        bsdel(visited, t->tid);
	return p - buf;
}

char *
tyfmt(char *buf, size_t len, Type *t)
{
	Bitset *bs;

	bs = mkbs();
	tybfmt(buf, len, bs, t);
	bsfree(bs);
	return buf;
}

char *
traitstr(Type *t)
{
	char buf[1024];
	traitfmt(buf, 1024, t);
	return strdup(buf);
}

char *
tystr(Type *t)
{
	char buf[1024];
	tyfmt(buf, 1024, t);
	return strdup(buf);
}

ulong
tyhash(void *ty)
{
	size_t i;
	Type *t;
	ulong hash;

	t = (Type *)ty;
	switch (t->type) {
	case Tyvar:	hash = inthash(t->tid);	break;
	case Tyunion:	hash = inthash(t->type);	break;
	case Tystruct:	hash = inthash(t->type);	break;
	case Tyname:	hash = namehash(t->name);	break;
	case Typaram:	hash = strhash(t->pname);	break;
	default: hash = inthash(t->type); break;
	}

	for (i = 0; i < t->narg; i++)
		hash ^= tyhash(t->arg[i]);
	return hash;
}

static ulong
typairhash(void *pp)
{
	Typair *p;

	p = pp;
	return inthash(p->atid) ^ inthash(p->btid);
}

static int
typaireq(void *a, void *b)
{
	Typair *pa, *pb;
	pa = a;
	pb = b;
	return pa->atid == pb->atid && pa->btid == pb->btid;
}

static void
equate(int32_t ta, int32_t tb)
{
	Typair *pa, *pb;

	/*
	 * unfortunately, we can't negatively cache
	 * here because tyvars may unify down and
	 * eventually make the negative result inaccurate.
	 */
	pa = zalloc(sizeof(Typair));
	*pa = (Typair){ta, tb};
	pb = zalloc(sizeof(Typair));
	*pb = (Typair){ta, tb};

	htput(eqcache, pa, pa);
	htput(eqcache, pb, pb);
}

int
tyeq_rec(Type *a, Type *b, Bitset *avisited, Bitset *bvisited, int search)
{
	Type *x, *y, *t;
	Typair p;
	size_t i;
	int ret;

	if (!a || !b)
		return a == b;
	if (search) {
		a = tysearch(a);
		b = tysearch(b);
		if ((t = boundtype(a)) != NULL)
			a = tysearch(t);
		if ((t = boundtype(b)) != NULL)
			b = tysearch(t);
	}
	if (a->type != b->type)
		return 0;
	if (a->narg != b->narg)
		return 0;
	if (a->nsub != b->nsub)
		return 0;
	if (a->nmemb != b->nmemb)
		return 0;
	p = (Typair){a->tid, b->tid};
	if (hthas(eqcache, &p))
		return 1;

	if (a->tid == b->tid)
		return 1;

	if (bshas(avisited, a->tid) && bshas(bvisited, b->tid))
		return 1;
	if (bshas(bvisited, a->tid) && bshas(avisited, b->tid))
		return 1;

	bsput(avisited, a->tid);
	bsput(bvisited, b->tid);
	ret = 1;

	switch (a->type) {
	case Typaram:
		ret = (a == b);
		ret = ret || streq(a->pname, b->pname);
		//if (ret != streq(a->pname, b->pname))
		//	die("wat");
		break;
	case Tyvar:
		if (a->tid != b->tid)
			ret = 0;
		break;
	case Tyunres:
		if (!nameeq(a->name, b->name))
			ret = 0;
	case Tyunion:
		for (i = 0; i < a->nmemb; i++) {
			if (!nameeq(a->udecls[i]->name, b->udecls[i]->name))
				ret = 0;
			x = a->udecls[i]->etype;
			y = b->udecls[i]->etype;
			if (!tyeq_rec(x, y, avisited, bvisited, search))
				ret = 0;
			if (!ret)
				break;
		}
		break;
	case Tystruct:
		for (i = 0; i < a->nmemb; i++) {
			if (strcmp(declname(a->sdecls[i]), declname(b->sdecls[i])) != 0)
				ret = 0;
			x = decltype(a->sdecls[i]);
			y = decltype(b->sdecls[i]);
			if (!tyeq_rec(x, y, avisited, bvisited, search))
				ret = 0;
			if (!ret)
				break;
		}
		break;
	case Tygeneric:
		if (!nameeq(a->name, b->name))
			ret = 0;
		for (i = 0; i < a->ngparam; i++) {
			ret = ret && tyeq_rec(a->gparam[i], b->gparam[i], avisited, bvisited, search);
			if (!ret)
				break;
		}
		break;
	case Tyname:
		if (!nameeq(a->name, b->name))
			ret = 0;
		for (i = 0; i < a->narg; i++) {
			ret = ret && tyeq_rec(a->arg[i], b->arg[i], avisited, bvisited, search);
			if (!ret)
				break;
		}
		break;
	case Tyarray:
		if (arraysz(a->asize) != arraysz(b->asize))
			ret = 0;
		break;
	default:
		break;
	}
	if (ret) {
		for (i = 0; i < a->nsub; i++) {
			x = a->sub[i];
			y = b->sub[i];
			if (!tyeq_rec(x, y, avisited, bvisited, search)) {
				ret = 0;
				break;
			}
		}
	}
	if (ret)
		equate(a->tid, b->tid);
	bsdel(avisited, a->tid);
	bsdel(bvisited, b->tid);

	return ret;
}

int
tystricteq(void *a, void *b)
{
	Bitset *avisited, *bvisited;
	int eq;

	if (a == b)
		return 1;
	avisited = mkbs();
	bvisited = mkbs();
	eq = tyeq_rec(a, b, avisited, bvisited, 0);
	bsfree(avisited);
	bsfree(bvisited);
	return eq;
}

int
tyeq(void *a, void *b)
{
	Bitset *avisited, *bvisited;
	int eq;

	if (a == b)
		return 1;
	avisited = mkbs();
	bvisited = mkbs();
	eq = tyeq_rec(a, b, avisited, bvisited, 1);
	bsfree(avisited);
	bsfree(bvisited);
	return eq;
}

size_t
tyidfmt(char *buf, size_t sz, Type *ty)
{
	size_t i;
	char *p, *end;

	ty = tysearch(ty);
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

void
iterableinit(Stab *st, Trait *tr)
{
	Node *func, *arg, **args;
	Type *ty;
	size_t nargs;

	/* trait iter @a -> @b = ... */
	tr->param = mktyparam(Zloc, "a");
	tr->aux = malloc(sizeof(Type*));
	tr->aux[0] = mktyparam(Zloc, "b");
	tr->naux = 1;

	/* __iternext__ : (it : @a#, outval : @b# -> bool) */
	args = NULL;
	nargs = 0;
	arg = mkdecl(Zloc, mkname(Zloc, "iter"), mktyptr(Zloc, mktyparam(Zloc, "a")));
	lappend(&args, &nargs, arg);
	arg = mkdecl(Zloc, mkname(Zloc, "ret"), mktyptr(Zloc, mktyparam(Zloc, "b")));
	lappend(&args, &nargs, arg);
	ty = mktyfunc(Zloc, args, nargs, mktype(Zloc, Tybool));

	func = mkdecl(Zloc, mkname(Zloc, "__iternext__"), ty);
	func->decl.trait = tr;
	func->decl.impls = mkht(tyhash, tyeq);
	func->decl.isgeneric = 1;
	func->decl.isconst = 1;
	func->decl.isglobl = 1;
	func->decl.isextern = 1;

	lappend(&tr->proto, &tr->nproto, func);
	putdcl(st, func);

	/* __iterfin__ : (it : @a#, outval : @b# -> void) */
	args = NULL;
	nargs = 0;
	arg = mkdecl(Zloc, mkname(Zloc, "iter"), mktyptr(Zloc, mktyparam(Zloc, "a")));
	lappend(&args, &nargs, arg);
	arg = mkdecl(Zloc, mkname(Zloc, "val"), mktyptr(Zloc, mktyparam(Zloc, "b")));
	lappend(&args, &nargs, arg);
	ty = mktyfunc(Zloc, args, nargs, mktype(Zloc, Tyvoid));

	func = mkdecl(Zloc, mkname(Zloc, "__iterfin__"), ty);
	func->decl.trait = tr;
	func->decl.impls = mkht(tyhash, tyeq);
	func->decl.isgeneric = 1;
	func->decl.isconst = 1;
	func->decl.isglobl = 1;
	func->decl.isextern = 1;

	lappend(&tr->proto, &tr->nproto, func);
	putdcl(st, func);
}

void
disposableinit(Stab *st, Trait *tr)
{
	Node *func, *arg, **args;
	Type *ty;
	size_t nargs;

	tr->param = mktyparam(Zloc, "a");
	tr->naux = 0;

	/* __dispose__ : (val : @a -> void) */
	args = NULL;
	nargs = 0;
	arg = mkdecl(Zloc, mkname(Zloc, "val"), mktyparam(Zloc, "a"));
	lappend(&args, &nargs, arg);
	ty = mktyfunc(Zloc, args, nargs, mktype(Zloc, Tyvoid));

	func = mkdecl(Zloc, mkname(Zloc, "__dispose__"), ty);
	func->decl.trait = tr;
	func->decl.impls = mkht(tyhash, tyeq);
	func->decl.isgeneric = 1;
	func->decl.isconst = 1;
	func->decl.isglobl = 1;
	func->decl.isextern = 1;

	lappend(&tr->proto, &tr->nproto, func);
	putdcl(st, func);
}

void
tyinit(Stab *st)
{
	Type *ty;
	Trait *tr;

	eqcache = mkht(typairhash, typaireq);
	tydeduptab = mkht(tyhash, tystricteq);
	/* this must be done after all the types are created, otherwise we will
	 * clobber the memoized bunch of types with the type params. */
#define Tc(c, n) \
	tr = mktrait(Zloc, \
		mkname(Zloc, n), NULL, \
		NULL, 0, \
		NULL, 0, \
		0); \
	puttrait(st, tr->name, tr);
#include "trait.def"
#undef Tc

	/* Definining and registering the types has to go after we define the
	 * constraints, otherwise they will have no constraints set on them. */
#define Ty(t, n, stk)		\
	if (t != Ntypes) {	\
		ty = mktype(Zloc, t);	\
		if (n) {	\
			puttype(st, mkname(Zloc, n), ty);	\
			htput(tydeduptab, ty, ty); \
		}	\
	}
#include "types.def"
#undef Ty

	/*
	 * finally, initializing the builtin traits for use in user code
	 * comes last, since this needs both the types and the traits set up
	 */
	iterableinit(st, traittab[Tciter]);
	disposableinit(st, traittab[Tcdisp]);

}
