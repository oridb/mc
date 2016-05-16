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
#include "mi.h"
#include "qasm.h"
#include "../config.h"

Type *tyintptr;
Type *tyword;
Type *tyvoid;
Node *abortoob;

typedef struct Gen Gen;
struct Gen {
	FILE *file;
	Cfg *cfg;
	char **typenames;
	Htab *strtab;

	Node *retval;
	Node *retlbl;
};

void pr(Gen *g, char *fmt, ...);
void out(Gen *g, char *fmt, ...);
void outqbetype(Gen *g, Type *ty);
Node *rval(Gen *g, Node *n, Node *dst);
Node *lval(Gen *g, Node *n);
static Node *slicebase(Gen *g, Node *sl, Node *off);
static Node *slicelen(Gen *g, Node *sl);
static Node *gencast(Gen *g, Node *val, Type *to);


static int isexport(Node *dcl)
{
	Node *n;

	/* Vishidden should also be exported. */
	if (dcl->decl.vis != Visintern)
		return 1;
	n = dcl->decl.name;
	if (!n->name.ns && streq(n->name.name, "main"))
		return 1;
	if (streq(n->name.name, "__init__"))
		return 1;
	return 0;
}

char *asmname(char *buf, size_t nbuf, Node *n, char sigil)
{
	char *ns, *name, *sep;

	ns = n->name.ns;
	name = n->name.name;
	sep = "";
	ns = "";
	if (ns && ns[0])
		sep = "$";
	bprintf(buf, nbuf, "%c%s%s%s", sigil, ns, sep, name);
	return buf;
}

Type *codetype(Type *ft)
{
	ft = tybase(ft);
	if (ft->type == Tycode)
		return ft;
	assert(ft->type == Tyfunc);
	ft = tydup(ft);
	ft->type = Tycode;
	return ft;
}

static Node *ptrsized(Gen *g, Node *v)
{
	if (size(v) == Ptrsz)
		return v;
	else
		return gencast(g, v, tyintptr);
}

char *qbetag(Gen *g, Type *ty)
{
	switch (ty->type) {
	case Tybool:	return "w";	break;
	case Tychar:	return "w";	break;
	case Tyint8:	return "w";	break;
	case Tyint16:	return "w";	break;
	case Tyint:	return "w";	break;
	case Tyint32:	return "w";	break;
	case Tyint64:	return "l";	break;
	case Tybyte:	return "w";	break;
	case Tyuint8:	return "w";	break;
	case Tyuint16:	return "w";	break;
	case Tyuint:	return "w";	break;
	case Tyuint32:	return "w";	break;
	case Tyuint64:	return "l";	break;
	case Tyflt32:	return "s";	break;
	case Tyflt64:	return "d";	break;
	case Typtr:	return "l";	break;
	default:	return "l";	break;
	}
}

char *_qbetype(Gen *g, Type *ty)
{
	switch (ty->type) {
	case Tybool:	return "b";	break;
	case Tychar:	return "w";	break;
	case Tyint8:	return "b";	break;
	case Tyint16:	return "h";	break;
	case Tyint:	return "w";	break;
	case Tyint32:	return "w";	break;
	case Tyint64:	return "l";	break;
	case Tybyte:	return "b";	break;
	case Tyuint8:	return "b";	break;
	case Tyuint16:	return "h";	break;
	case Tyuint:	return "w";	break;
	case Tyuint32:	return "w";	break;
	case Tyuint64:	return "l";	break;
	case Tyflt32:	return "s";	break;
	case Tyflt64:	return "d";	break;
	case Typtr:	return "l";	break;
	default:	return g->typenames[ty->tid];	break;
	}
}

void fillglobls(Stab *st, Htab *globls)
{
	char buf[1024];
	size_t i, j, nk, nns;
	void **k, **ns;
	Stab *stab;
	Node *s;

	k = htkeys(st->dcl, &nk);
	for (i = 0; i < nk; i++) {
		s = htget(st->dcl, k[i]);
		if (isconstfn(s))
			s->decl.type = codetype(s->decl.type);
		asmname(buf, sizeof buf, s->decl.name, '$');
		htput(globls, s, strdup(buf));
	}
	free(k);

	ns = htkeys(file->file.ns, &nns);
	for (j = 0; j < nns; j++) {
		stab = htget(file->file.ns, ns[j]);
		k = htkeys(stab->dcl, &nk);
		for (i = 0; i < nk; i++) {
			s = htget(stab->dcl, k[i]);
			asmname(buf, sizeof buf, s->decl.name, '$');
			htput(globls, s, strdup(buf));
		}
		free(k);
	}
	free(ns);
}

static void initconsts(Htab *globls)
{
	Type *ty;
	Node *name;
	Node *dcl;
	char buf[1024];

	tyintptr = mktype(Zloc, Tyuint64);
	tyword = mktype(Zloc, Tyuint);
	tyvoid = mktype(Zloc, Tyvoid);

	ty = mktyfunc(Zloc, NULL, 0, mktype(Zloc, Tyvoid));
	ty->type = Tycode;
	name = mknsname(Zloc, "_rt", "abort_oob");
	dcl = mkdecl(Zloc, name, ty);
	dcl->decl.isconst = 1;
	dcl->decl.isextern = 1;
	dcl->decl.isglobl = 1;
	asmname(buf, sizeof buf, dcl->decl.name, '$');
	htput(globls, dcl, strdup(buf));

	abortoob = mkexpr(Zloc, Ovar, name, NULL);
	abortoob->expr.type = ty;
	abortoob->expr.did = dcl->decl.did;
	abortoob->expr.isconst = 1;
}

static Node *tyloctemp(Gen *g, Srcloc loc, Type *ty)
{
	Node *t, *dcl;

	t = NULL;
	if (isstacktype(ty)) {
		t = gentemp(loc, mktyptr(loc, ty), &dcl);
		out(g, "%v =l alloc4 %l\n", t, tysize(ty));
	} else {
		t = gentemp(loc, ty, &dcl);
	}
	return t;
}

static Node *tytemp(Gen *g, Type *ty)
{
	return tyloctemp(g, ty->loc, ty);
}

static Node *temp(Gen *g, Node *e)
{
	return tyloctemp(g, e->loc, exprtype(e));
}

void putlit(Gen *g, Node *n, FILE *f)
{
	char buf[64];

	n = n->expr.args[0];
	switch (n->lit.littype) {
	case Lchr:	fprintf(f, "%ld", (long)n->lit.chrval);	break;
	case Lbool:	fprintf(f, "%d", n->lit.boolval);	break;
	case Lint:	fprintf(f, "%lld", (vlong)n->lit.intval);	break;
	case Lflt:	fprintf(f, "d_%g", n->lit.fltval);	break;
	case Lfunc:	fprintf(f, "f$anon$%d", n->lit.fnval->nid);	break;
	case Lstr:
		if (!hthas(g->strtab, &n->lit.strval)) {
			genlblstr(buf, sizeof buf, "str");
			htput(g->strtab, &n->lit.strval, strdup(buf));
		}
		fprintf(f, "$%s", (char*)htget(g->strtab, &n->lit.strval));
		break;
	case Llbl:
		fprintf(f, "@%s", n->lit.lblval);
		break;
	case Lvoid:
		die("lit not puttable");
	}
}

void putnode(Gen *g, Node *n, FILE *f)
{
	char buf[1024];
	Node *dcl;
	char sigil;

	switch (n->type) {
	case Ndecl:
		sigil = '%';
		if (n->decl.isglobl)
		      sigil = '$';
		asmname(buf, sizeof buf, n->decl.name, sigil);
		fprintf(f, "%s", buf);
		break;
	case Nexpr:
		if (exprop(n) == Ovar) {
		      dcl = decls[n->expr.did];
		      sigil = '%';
		      if (dcl->decl.isglobl)
			    sigil = '$';
		      asmname(buf, sizeof buf, n->expr.args[0], sigil);
		      fprintf(f, "%s", buf);
		} else if (exprop(n) == Olit) {
			putlit(g, n, f);
		} else {
		      die("out invalid expr?");
		}
		break;
	default:
		die("bad node type %s", nodestr[n->type]);
	}
}


void out(Gen *g, char *fmt, ...)
{
	char *p;
	va_list ap;
	Type *t;

	va_start(ap, fmt);
	for (p = fmt; *p; p++) {
		if (*p != '%') {
			fputc(*p, g->file);
			continue;
		}
		p++;
		switch (*p) {
		case '%':	fputc('%', g->file);	break;
		case 's':	fputs(va_arg(ap, char*), g->file);	break;
		case 'v':	putnode(g, va_arg(ap, Node*), g->file);	break;
		case 'd':	fprintf(g->file, "%d", va_arg(ap, int)); break;
		case 'l':	fprintf(g->file, "%lld", (vlong)va_arg(ap, int64_t)); break;
		case 't':	
				
				t = va_arg(ap, Type*);
				fputs(qbetag(g, t), g->file);	break;
		case 'T':	
				
				t = va_arg(ap, Type*);
				fputs(_qbetype(g, t), g->file);	break;
		default:	die("bad format character %c", *p);	break;
		}
	}
}

void pr(Gen *g, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(g->file, fmt, ap);
	va_end(ap);
}

Node *binop(Gen *g, char *op, Node *l, Node *r)
{
	Node *t;

	t = temp(g, l);
	l = rval(g, l, NULL);
	r = rval(g, r, NULL);
	out(g, "\t%v =%t %s %v, %v\n", t, exprtype(t), op, l, r);
	return t;
}

Node *binopk(Gen *g, char *op, Node *l, uvlong r)
{
	Node *t;

	t = temp(g, l);
	l = rval(g, l, NULL);
	out(g, "\t%v =%t %s %v, %l\n", t, exprtype(t), op, l, r);
	return t;
}

static Node *load(Gen *g, Node *ptr, Type *ty)
{
	Node *tmp, *dcl;

	tmp = gentemp(ptr->loc, ty, &dcl);
	out(g, "\t%v =%t load%t %v", tmp, ty, ptr);
	return tmp;
}

static Node *getcode(Gen *g, Node *n)
{
	Node *r, *d;
	Type *ty;

	if (isconstfn(n)) {
		d = decls[n->expr.did];
		r = mkexpr(n->loc, Ovar, n->expr.args[0], NULL);
		r->expr.did = d->decl.did;
		r->expr.type = codetype(exprtype(n));
	} else {
		ty = tybase(exprtype(n));
		assert(ty->type == Tyfunc);
		r = load(g, binopk(g, "add", n, Ptrsz), tyintptr);
	}
	return r;
}

static Node *slicebase(Gen *g, Node *sl, Node *off)
{
	Node *u, *v;
	Type *ty;
	size_t sz;

	sl = rval(g, sl, NULL);
	ty = tybase(exprtype(sl));
	switch (ty->type) {
	case Typtr:	u = sl;	break;
	case Tyarray:	u = sl;	break;
	case Tyslice:	u = load(g, sl, mktyptr(ty->loc, ty->sub[0]));	break;
	default: die("Unslicable type %s", tystr(sl->expr.type));
	}
	/* safe: all types we allow here have a sub[0] that we want to grab */
	if (off) {
		off = ptrsized(g, rval(g, off, NULL));
		sz = tysize(sl->expr.type->sub[0]);
		v = binopk(g, "mul", off, sz);
		return binop(g, "add", u, v);
	} else {
		return u;
	}
}

static Node *slicelen(Gen *g, Node *sl)
{
	Node *szp;

	szp = binopk(g, "add", sl, Ptrsz);
	return load(g, szp, tyintptr);
}

static Node *seqlen(Gen *g, Node *n, Type *ty)
{
	Node *t, *r;

	ty = tybase(exprtype(n));
	r = NULL;
	if (ty->type == Tyslice) {
		t = slicelen(g, n);
		r = gencast(g, t, ty);
	} else if (ty->type == Tyarray) {
		t = exprtype(n)->asize;
                if (t)
                    r = gencast(g, t, ty);
	} else if (ty->type == Typtr) {
		return NULL;
	} else {
		die("invalid type for seqlen");
	}
	return r;
}

Node *cmpop(Gen *g, char *op, Node *l, Node *r)
{
	Node *t;
	Type *ty;

	t = tytemp(g, tyword);
	ty = exprtype(t);
	l = rval(g, l, NULL);
	r = rval(g, r, NULL);
	out(g, "\t%v =%t c%s%t %v, %v\n", t, ty, op, exprtype(l), l, r);
	return t;
}

static Node *intconvert(Gen *g, Node *val, Type *to, int signconv)
{
	Node *t;
	char *sign;

	t = tytemp(g, to);

	sign = "u";
	if (signconv)
		sign = "s";

	if (tysize(to) > size(val))
		out(g, "\t%v =%t ext%s%t %v", t, to, sign, exprtype(val), val);
	else
		out(g, "\t%v =%t copy %v", t, to, exprtype(val), val);
	return t;
}

static Node *gencast(Gen *g, Node *val, Type *to)
{
	Node *r;
	Type *t;

	r = NULL;
	/* do the type conversion */
	switch (tybase(to)->type) {
	case Tybool:
	case Tyint8: case Tyint16: case Tyint32: case Tyint64:
	case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
	case Tyint: case Tyuint: case Tychar: case Tybyte:
	case Typtr:
		t = tybase(exprtype(val));
		switch (t->type) {
		/* ptr -> slice conversion is disallowed */
		case Tyslice:
			if (tybase(to)->type != Typtr)
				fatal(val, "bad cast from %s to %s", tystr(exprtype(val)), tystr(to));
			val = rval(g, val, NULL);
			r = slicebase(g, val, NULL);
			break;
		case Tyfunc:
			if (to->type != Typtr)
				fatal(val, "bad cast from %s to %s", tystr(exprtype(val)), tystr(to));
			r = getcode(g, val);
			break;
		/* signed conversions */
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyint:
			r = intconvert(g, val, to, 1);
			break;
		/* unsigned conversions */
		case Tybool:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyuint: case Tychar: case Tybyte:
		case Typtr:
			r = intconvert(g, val, to, 0);
			break;
		case Tyflt32: case Tyflt64:
			if (tybase(to)->type == Typtr)
				fatal(val, "bad cast from %s to %s",
						tystr(exprtype(val)), tystr(to));
			r = mkexpr(val->loc, Oflt2int, rval(g, val, NULL), NULL);
			r->expr.type = to;
			break;
		default:
			fatal(val, "bad cast from %s to %s",
					tystr(exprtype(val)), tystr(to));
		}
		break;
	case Tyflt32: case Tyflt64:
		t = tybase(exprtype(val));
		switch (t->type) {
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyint: case Tyuint: case Tychar: case Tybyte:
			r = mkexpr(val->loc, Oint2flt, rval(g, val, NULL), NULL);
			r->expr.type = to;
			break;
		case Tyflt32: case Tyflt64:
			r = mkexpr(val->loc, Oflt2flt, rval(g, val, NULL), NULL);
			r->expr.type = to;
			break;
		default:
			fatal(val, "bad cast from %s to %s",
					tystr(exprtype(val)), tystr(to));
			break;
		}
		break;
		/* no other destination types are handled as things stand */
	default:
		fatal(val, "bad cast from %s to %s",
				tystr(exprtype(val)), tystr(to));
	}
	return r;
}


void blit(Gen *g, Node *dst, Node *src)
{
	die("unimplemented blit");
}

Node *assign(Gen *g, Node *dst, Node *src)
{
	dst = lval(g, dst);
	src = rval(g, src, dst);
	if (src != dst) {
		if (isstacktype(exprtype(dst)))
			blit(g, dst, src);
		else
			out(g, "\t%v =%t copy %v\n", dst, exprtype(dst), src);
	}
	return dst;
}

static void checkidx(Gen *g, char *op, Node *len, Node *idx)
{
	Node *inrange, *ok, *fail;

	if (!len)
		return;
	ok = genlbl(len->loc);
	fail = genlbl(len->loc);
	inrange = binop(g, op, len, idx);
	out(g, "\tjnz %v, %v, %v\n", inrange, ok, fail);
	out(g, "%v", fail);
	out(g, "\tcall %_rt$abort_oob()\n");
	out(g, "%v", ok);
}

Node *lval(Gen *g, Node *n)
{
	Node *r;
	//Node **args;

	//args = n->expr.args;
	switch (exprop(n)) {
	case Ovar:	r = n;	break;
	//case Oidx:	r = loadidx(g, args[0], args[1]);	break;
	//case Oderef:	r = deref(rval(s, args[0], NULL), NULL);	break;
	case Omemb:	r = rval(g, n, NULL);	break;
	case Ostruct:	r = rval(g, n, NULL);	break;
	case Oucon:	r = rval(g, n, NULL);	break;
	case Oarr:	r = rval(g, n, NULL);	break;
	case Ogap:	r = temp(g, n);	break;

	/* not actually expressible as lvalues in syntax, but we generate them */
	case Oudata:	r = rval(g, n, NULL);	break;
	case Outag:	r = rval(g, n, NULL);	break;
	case Otupget:	r = rval(g, n, NULL);	break;
	default:
			fatal(n, "%s cannot be an lvalue", opstr[exprop(n)]);
			break;
	}
	return r;
}

Node *gencall(Gen *g, Node *n)
{
	Node *ret, *fn;
	Node **args;
	size_t nargs, i;

	nargs = n->expr.nargs;
	args = malloc(nargs * sizeof(Node *));

	if (tybase(exprtype(n))->type != Tyvoid)
		ret = temp(g, n);
	else
		ret = NULL;

	for (i = 1; i < nargs; i++)
		args[i] = rval(g, n->expr.args[i], NULL);
	fn = rval(g, n->expr.args[0], NULL);

	if (ret)
		out(g, "\t%v =%t call %v (", ret, exprtype(ret), fn);
	else
		out(g, "\tcall %v (", ret, fn);
	for (i = 1; i < nargs; i++)
		out(g, "%t %v", exprtype(args[i]), args[i]);
	out(g, ")\n");
	free(args);
	return ret;
}

Node *genslice(Gen *g, Node *sl, Node *dst)
{
	Node *lo, *hi, *lim, *base, *len;
	Node *basep, *lenp;
	Node *seq;

	if (!dst)
		dst = temp(g, sl);
	basep = dst;
	lenp = binopk(g, "add", dst, Ptrsz);

	seq = rval(g, sl->expr.args[0], NULL);
	/*
	 * special case: the ptr arg of slices off the empty array need to be zeroed,
	 * so that the allocator can be happy.
	 */
	if (exprop(seq) == Oarr && seq->expr.nargs == 0) {
		base = mkintlit(sl->loc, 0);
		base->expr.type = tyintptr;
	} else {
		seq = rval(g, seq, NULL);
		base = slicebase(g, seq, sl->expr.args[0]);
	}
	lo = ptrsized(g, rval(g, sl->expr.args[1], NULL));
	hi = ptrsized(g, rval(g, sl->expr.args[2], NULL));
	len = binop(g, "sub", hi, lo);
	lim = seqlen(g, seq, tyword);
	if (lim)
		checkidx(g, "cle", lim, hi);

	out(g, "\tstor%t %v, %v\n", basep, base);
	out(g, "\tstor%t %v, %v\n", lenp, len);
	return dst;
}

Node *rval(Gen *g, Node *n, Node *dst)
{
	Node *r; /* expression result */
	Node **args;
	Type *ty;

	r = NULL;
	args = n->expr.args;
	switch (exprop(n)) {
	/* arithmetic */
	case Oadd:	r = binop(g, "add", args[0], args[1]);	break;
	case Osub:	r = binop(g, "sub", args[0], args[1]);	break;
	case Omul:	r = binop(g, "mul", args[0], args[1]);	break;
	case Odiv:	r = binop(g, "div", args[0], args[1]);	break;
	case Omod:	r = binop(g, "mod", args[0], args[1]);	break;
	case Ofadd:	r = binop(g, "add", args[0], args[1]);	break;
	case Ofsub:	r = binop(g, "sub", args[0], args[1]);	break;
	case Ofmul:	r = binop(g, "mul", args[0], args[1]);	break;
	case Ofdiv:	r = binop(g, "div", args[0], args[1]);	break;
	case Ofneg:	r = binop(g, "neg", args[0], args[1]);	break;
	case Obor:	r = binop(g, "or", args[0], args[1]);	break;
	case Oband:	r = binop(g, "and", args[0], args[1]);	break;
	case Obxor:	r = binop(g, "xor", args[0], args[1]);	break;
	case Obsl:	r = binop(g, "shl", args[0], args[1]);	break;
	case Obsr:	
			if (istysigned(exprtype(n)))
				r = binop(g, "shr", args[0], args[1]);
			else
				r = binop(g, "shl", args[0], args[1]);
			break;
	case Obnot:	die("what's the operator for negate bits?\n");

	/* equality */
	case Oeq:	r = cmpop(g, "eq", args[0], args[1]);	break;
	case One:	r = cmpop(g, "ne", args[0], args[1]);	break;
	case Ogt:	r = cmpop(g, "sgt", args[0], args[1]);	break;
	case Oge:	r = cmpop(g, "sge", args[0], args[1]);	break;
	case Olt:	r = cmpop(g, "slt", args[0], args[1]);	break;
	case Ole:	r = cmpop(g, "sle", args[0], args[1]);	break;
	case Ofeq:	r = cmpop(g, "eq", args[0], args[1]);	break;
	case Ofne:	r = cmpop(g, "ne", args[0], args[1]);	break;
	case Ofgt:	r = cmpop(g, "gt", args[0], args[1]);	break;
	case Ofge:	r = cmpop(g, "ge", args[0], args[1]);	break;
	case Oflt:	r = cmpop(g, "lt", args[0], args[1]);	break;
	case Ofle:	r = cmpop(g, "le", args[0], args[1]);	break;
	case Oueq:	r = cmpop(g, "eq", args[0], args[1]);	break;
	case Oune:	r = cmpop(g, "ne", args[0], args[1]);	break;
	case Ougt:	r = cmpop(g, "ugt", args[0], args[1]);	break;
	case Ouge:	r = cmpop(g, "uge", args[0], args[1]);	break;
	case Oult:	r = cmpop(g, "ult", args[0], args[1]);	break;
	case Oule:	r = cmpop(g, "ule", args[0], args[1]);	break;

	case Oasn:	r = assign(g, args[0], args[1]);	break;
	case Ovar:	r = n;	break;
	case Olit:	r = n;	break;
	case Osize:
		r = mkintlit(n->loc, size(args[0]));
		r->expr.type = exprtype(n);
		break;
	case Ocall:
		r = gencall(g, n);
		break;

	case Oret:
		ty = tybase(exprtype(args[0]));
		if (ty->type != Tyvoid)
			assign(g, g->retval, args[0]);
		out(g, "\tjmp %v\n", g->retlbl);
		break;

	case Oderef:
		ty = tybase(exprtype(args[0]));
		if (isstacktype(ty)) {
			r = rval(g, args[0], args[0]);
		} else {
			r = temp(g, args[0]);
			out(g, "\t%v =%t load%t %v\n", r, ty, ty, args[0]);
		}
		break;
	case Oaddr:
		ty = tybase(exprtype(args[0]));
		if (isstacktype(ty)) {
			r = rval(g, args[0], args[0]);
		} else {
			die("taking address of non-addressable value");
		}
		break;

	case Ojmp:
		out(g, "\tjmp %v\n", args[0]);
		break;
	case Ocjmp:
		out(g, "\tjnz %v, %v, %v\n", rval(g, args[0], NULL), args[1], args[2]);
		break;

	case Oslice:
		r = genslice(g, n, dst);
		break;

	case Oidx:
	case Omemb:
	case Osllen:
	case Oucon:
	case Outag:
	case Oudata:
	case Otup:
	case Oarr:
	case Ostruct:
	case Ocast:
	case Ogap:
	case Oneg:
	case Otupget:
	case Olnot:
	case Ovjmp:
	case Oset:
	case Oslbase:
	case Oblit:
	case Oclear:
	case Ocallind:
	case Otrunc:
	case Ozwiden:
	case Oswiden:
	case Oflt2int:
	case Oint2flt:
	case Oflt2flt:
                die("unimplemented operator %s", opstr[exprop(n)]);

	case Odead:
	case Oundef:
	case Odef:
		break;

	/* should be dealt with earlier */
	case Olor: case Oland:
	case Oaddeq: case Osubeq: case Omuleq: case Odiveq: case Omodeq:
	case Oboreq: case Obandeq: case Obxoreq: case Obsleq: case Obsreq:
	case Opreinc: case Opredec: case Opostinc: case Opostdec:
	case Obreak: case Ocontinue:
	case Numops:
                die("invalid operator: should have been removed");
	case Obad:
		die("bad operator");
		break;
	}
	return r;
}

void genbb(Gen *g, Cfg *cfg, Bb *bb)
{
	size_t i;

	for (i = 0; i < bb->nlbls; i++)
		pr(g, "@%s\n", bb->lbls[i]);

	for (i = 0; i < bb->nnl; i++) {
		switch (bb->nl[i]->type) {
		case Ndecl:
			//declarelocal(s, n);
			break;
		case Nexpr:
			rval(g, bb->nl[i], NULL);
			break;
		default:
			dump(bb->nl[i], stderr);
			die("bad node passsed to simp()");
			break;
		}

	}
}

int abileaks(Type *ty)
{
	switch (tybase(ty)->type) {
	case Tybool:
	case Tychar:
	case Tyint8:
	case Tyint16:
	case Tyint:
	case Tyint32:
	case Tyint64:
	case Tybyte:
	case Tyuint8:
	case Tyuint16:
	case Tyuint:
	case Tyuint32:
	case Tyuint64:
	case Tyflt32:
	case Tyflt64:
	case Typtr:
		return 1;
	default:
		return 0;
	}
}

void funcargs(Gen *g, Node *fn)
{
	Node *a;
	size_t i;

	pr(g, "(");
	for (i = 0; i < fn->func.nargs; i++) {
		a = fn->func.args[i];
		if (abileaks(decltype(a)))
			out(g, "%t %v", decltype(a), a);
		else
			out(g, "%T %v", decltype(a), a);
	}
	pr(g, ")");
}

void genfn(Gen *g, Node *dcl)
{
	Node *n, *b, *retdcl;
	size_t i;
	Cfg *cfg;
	Bb *bb;
	Type *ret;
	char name[1024];


	n = dcl->decl.init;
	asmname(name, sizeof name, dcl->decl.name, '$');
	if(debugopt['i'] || debugopt['F'] || debugopt['f'])
		printf("\n\nfunction %s\n", name);

	/* set up the simp context */
	/* unwrap to the function body */
	//collectenv(s, n);
	n = n->expr.args[0];
	n = n->lit.fnval;
	b = n->func.body;
	//simpinit(s, n);
	cfg = mkcfg(dcl, b->block.stmts, b->block.nstmts);
	check(cfg);
	if (debugopt['t'] || debugopt['s'])
		dumpcfg(cfg, stdout);

	/* func declaration */
	if (isexport(dcl))
		pr(g, "export ");
	pr(g, "function ");
	ret = n->func.type->sub[0];
	g->retval = gentemp(dcl->loc, ret, &retdcl);
	g->retlbl = genlbl(dcl->loc);
	if (tybase(ret)->type != Tyvoid) {
		if (abileaks(ret))
			pr(g, "%s ", qbetag(g, ret));
		else
			pr(g, "%s ", _qbetype(g, ret));
	}
	pr(g, "%s", name);
	funcargs(g, n);

	/* func body */
	pr(g, "{\n");
	pr(g, "@start\n");
	for (i = 0; i < cfg->nbb; i++) {
		bb = cfg->bb[i];
		if (!bb)
			continue;
		genbb(g, cfg, bb);
	}
	out(g, "%v\n", g->retlbl);
	if (tybase(ret)->type == Tyvoid)
		out(g, "\tret\n");
	else
		out(g, "\tret %v\n", g->retval);
	pr(g, "}\n");
}

static void encodemin(Gen *g, uint64_t val)
{
	size_t i, shift;
	uint8_t b;

	if (val < 128) {
		pr(g, "\tb %zd\n", val);
		return;
	}

	for (i = 1; i < 8; i++)
		if (val < 1ULL << (7*i))
			break;
	shift = 8 - i;
	b = ~0ull << (shift + 1);
	b |= val & ~(~0ull << shift);
	pr(g, "\tb %u\n", b);
	val >>=  shift;
	while (val != 0) {
		pr(g, "\tb %u\n", (uint)val & 0xff);
		val >>= 8;
	}
}

static void outbytes(Gen *g, char *p, size_t sz)
{
	size_t i;

	for (i = 0; i < sz; i++) {
		if (i % 60 == 0)
			pr(g, "\t.ascii \"");
		if (p[i] == '"' || p[i] == '\\')
			pr(g, "\\");
		if (isprint(p[i]))
			pr(g, "%c", p[i]);
		else
			pr(g, "\\%03o", (uint8_t)p[i] & 0xff);
		/* line wrapping for readability */
		if (i % 60 == 59 || i == sz - 1)
			pr(g, "\"\n");
	}
}

void genblob(Gen *g, Blob *b)
{
	size_t i;

	if (b->lbl) {
		if (b->iscomdat)
			/* FIXME: emit once */
		if (b->isglobl)
			pr(g, "export ");
		pr(g, "$%s {\n", b->lbl);
	}

	switch (b->type) {
	case Btimin:	encodemin(g, b->ival);	break;
	case Bti8:	pr(g, "\tb %zd\n", b->ival);	break;
	case Bti16:	pr(g, "\th %zd\n", b->ival);	break;
	case Bti32:	pr(g, "\tw %zd\n", b->ival);	break;
	case Bti64:	pr(g, "\tl %zd\n", b->ival);	break;
	case Btbytes:	outbytes(g, b->bytes.buf, b->bytes.len);	break;
	case Btpad:	pr(g, "\tz %zd\n", b->npad);	break;
	case Btref:	pr(g, "\tl $%s + %zd\n", b->ref.str, b->ref.off);	break;
	case Btseq:
		for (i = 0; i < b->seq.nsub; i++)
			genblob(g, b->seq.sub[i]);
		break;
	}
	if(b->lbl) {
		pr(g, "}\n\n");
	}
}

void gendata(Gen *g, Node *data)
{
}

void gentydescblob(Gen *g, Type *ty)
{
	Blob *b;

	ty = tydedup(ty);
	if (ty->type == Tyvar || ty->isemitted)
		return;

	ty->isemitted = 1;
	b = tydescblob(ty);
	b->iscomdat = 1;
	genblob(g, b);
	blobfree(b);
}

void gentydesc(Gen *g)
{
	Type *ty;
	size_t i;

	for (i = Ntypes; i < ntypes; i++) {
		if (!types[i]->isreflect)
			continue;
		ty = tydedup(types[i]);
		if (ty->isemitted || ty->isimport)
			continue;
		gentydescblob(g, ty);
	}
	pr(g, "\n");
}

void outarray(Gen *g, Type *ty)
{
	size_t sz;

	sz = 0;
	if (ty->asize)
		sz = ty->asize->expr.args[0]->lit.intval;
	outqbetype(g, ty->sub[0]);
	pr(g, "\t%s %zd,\n", _qbetype(g, ty->sub[0]), sz);
}

void outstruct(Gen *g, Type *ty)
{
	size_t i;
	Type *mty;

	for (i = 0; i < ty->nmemb; i++) {
		mty = decltype(ty->sdecls[i]);
		outqbetype(g, mty);
		pr(g, "%s,\n", _qbetype(g, mty));
	}
}

void outunion(Gen *g, Type *ty)
{
	size_t i;
	Type *mty;
	size_t maxalign;
	size_t maxsize;

	maxalign = 1;
	maxsize = 0;
	for (i = 0; i < ty->nmemb; i++) {
		mty = ty->udecls[i]->etype;
		if (!mty)
			continue;
		if (tyalign(mty) > maxalign)
			maxalign = tyalign(mty);
		if (tysize(mty) > maxsize)
			maxsize = tysize(mty);
	}
	maxsize += align(4, maxalign);
	pr(g, "align %d { %d }\n", maxalign, maxsize);
}

void outtuple(Gen *g, Type *ty)
{
	size_t i;
	Type *mty;

	for (i = 0; i < ty->nsub; i++) {
		mty = ty->sub[i];
		outqbetype(g, mty);
		pr(g, "%s,\n", _qbetype(g, mty));
	}
}

void outqbetype(Gen *g, Type *ty)
{
	if (g->typenames[ty->tid])
		return;

	switch (ty->type) {
	case Tyvoid:	break;
	case Tybool:	pr(g, "\tb,\n");	break;
	case Tychar:	pr(g, "\tw,\n");	break;
	case Tyint8:	pr(g, "\tb,\n");	break;
	case Tyint16:	pr(g, "\th,\n");	break;
	case Tyint:	pr(g, "\tw,\n");	break;
	case Tyint32:	pr(g, "\tw,\n");	break;
	case Tyint64:	pr(g, "\tl,\n");	break;
	case Tybyte:	pr(g, "\tb,\n");	break;
	case Tyuint8:	pr(g, "\tb,\n");	break;
	case Tyuint16:	pr(g, "\th,\n");	break;
	case Tyuint:	pr(g, "\tw,\n");	break;
	case Tyuint32:	pr(g, "\tw,\n");	break;
	case Tyuint64:	pr(g, "\tl,\n");	break;
	case Tyflt32:	pr(g, "\ts,\n");	break;
	case Tyflt64:	pr(g, "\td,\n");	break;
	case Typtr:	pr(g, "\tl,\n");	break;
	case Tyslice:	pr(g, "\tl, l,\n");	break;
	case Tycode:	pr(g, "\tl,\n");	break;
	case Tyfunc:	pr(g, "\tl, l,\n");	break;
	case Tyvalist:	pr(g, "\tl\n");	break;
	case Tyarray:	outarray(g, ty);	break;
	case Tystruct:	outstruct(g, ty);	break;
	case Tytuple:	outtuple(g, ty);	break;
	case Tyunion:	outunion(g, ty);	break;
	case Tyname:	outqbetype(g, ty->sub[0]);	break;

	/* frontend/invalid types */
	case Tyvar:
	case Tybad:
	case Typaram:
	case Tyunres:
	case Tygeneric:
	case Ntypes:
			//die("invalid type %s");
			break;
	}
}

void genqbetypes(Gen *g)
{
	char buf[1024];
	size_t i;
	Type *ty;
	char *n;

	g->typenames = zalloc(ntypes * sizeof(Type *));
	for (i = Ntypes; i < ntypes; i++) {
		ty = tydedup(types[i]);
		if (ty->type == Tycode || ty->type == Tyvar || hasparams(ty))
			continue;
		if (ty->vis == Visbuiltin)
			continue;
		n = tystr(ty);
		if (ty->type == Tyname)
			pr(g, "type :%s = { # %s\n", asmname(buf, sizeof buf, ty->name, 0), n);
		else
			pr(g, "type :t%zd = { # %s\n", i, n);
		free(n);
		outqbetype(g, ty);
		pr(g, "}\n\n");
	}
}

void gen(Node *file, char *out)
{
	Htab *globls;
	Node *n;
	Gen *g;
	size_t i;

	/* set up code gen state */
	globls = mkht(varhash, vareq);
	initconsts(globls);
	fillglobls(file->file.globls, globls);

	/* generate the code */
	g = zalloc(sizeof(Gen));
	g->file = fopen(out, "w");
	if (!g->file)
		die("Couldn't open fd %s", out);
	g->strtab = mkht(strlithash, strliteq);

	genqbetypes(g);
	pushstab(file->file.globls);
	for (i = 0; i < file->file.nstmts; i++) {
		n = file->file.stmts[i];
		switch (n->type) {
		case Nuse:
			case Nimpl
				/* nothing to do */ :
				break;
		case Ndecl:
			if (isconstfn(n)) {
				n = flattenfn(n);
				genfn(g, n);
			} else {
				gendata(g, n);
			}
			break;
		default:
			die("Bad node %s in toplevel", nodestr[n->type]);
			break;
		}
	}
	popstab();
	gentydesc(g);
	fclose(g->file);
}
