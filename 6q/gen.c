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

typedef struct Gen Gen;
typedef struct Loc Loc;

typedef enum {
	Ldecl,
	Ltemp,
	Llabel,
	Lconst
} Loctype;


struct Loc {
	char tag;
	char kind;
	union {
		int64_t dcl;
		int64_t tmp;
		int64_t cst;
		char *lbl;
	};
};

struct Gen {
	FILE *file;
	Cfg *cfg;
	char **typenames;
	Htab *strtab;

	/* memory offsets/addrs */
	Htab *envoff;
	Htab *stkoff;
	Htab *globls;
	size_t stksz;

	Node *retval;
	Node *retlbl;
	Node **locals;
	size_t nlocals;
	size_t nexttmp;
};

char qtag(Gen *g, Type *ty);
static void pr(Gen *g, char *fmt, ...);
static void out(Gen *g, char *fmt, ...);
static void outtypebody(Gen *g, Type *ty);
static void outqtype(Gen *g, Type *ty);
static Loc rval(Gen *g, Node *n);
static Loc lval(Gen *g, Node *n);
static Loc slicebase(Gen *g, Node *sl, Node *off);
static Loc slicelen(Gen *g, Loc sl);
static Loc gencast(Gen *g, Srcloc loc, Loc val, Type *to, Type *from);
static Loc loadidx(Gen *g, Node *base, Node *idx);

Type *tyintptr;
Type *tyword;
Type *tyvoid;
Node *abortoob;
Loc Z;

int qtagsizes[256] = {
	['b'] = 1, 
	['h'] = 2,
	['w'] = 4,
	['l'] = 8
};

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
	if (!ns)
		ns = "";
	name = n->name.name;
	sep = "";
	if (ns && ns[0])
		sep = "$";
	bprintf(buf, nbuf, "%s%s%s%s", sigil, ns, sep, name);
	return buf;
}

Loc qtemp(Gen *g, char type)
{
	return (Loc){.kind=Ltemp, .dcl=g->nexttmp++, .tag=type};
}

Loc temp(Gen *g, Node *n)
{
	char type;

	if (n->type == Ndecl)
		type = qtag(g, decltype(n));
	else
		type = qtag(g, exprtype(n));
	return (Loc){.kind=Ltemp, .tmp=g->nexttmp++, .tag=type};
}

Loc qvar(Gen *g, Node *dcl)
{
	char tag;

	tag = qtag(g, dcl->decl.type);
	return (Loc){.kind=Ldecl, .dcl=dcl->decl.did, .tag=tag};
}

Loc qlabel(Gen *g, char *lbl, char tag)
{
	return (Loc){.kind=Lconst, .lbl=lbl, .tag=tag};
}

Loc qconst(Gen *g, uint64_t cst, char tag)
{
	return (Loc){.kind=Lconst, .cst=cst, .tag=tag};
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

static Loc ptrsized(Gen *g, Loc v)
{
	Loc r;

	if (v.tag == 'l')
		return v;
	r = qtemp(g, 'l');
	out(g, "\t%v =l copy %v", r, v);
	return r;
}

char qtag(Gen *g, Type *ty)
{
	switch (ty->type) {
	case Tybool:	return 'w';	break;
	case Tychar:	return 'w';	break;
	case Tyint8:	return 'w';	break;
	case Tyint16:	return 'w';	break;
	case Tyint:	return 'w';	break;
	case Tyint32:	return 'w';	break;
	case Tyint64:	return 'l';	break;
	case Tybyte:	return 'w';	break;
	case Tyuint8:	return 'w';	break;
	case Tyuint16:	return 'w';	break;
	case Tyuint:	return 'w';	break;
	case Tyuint32:	return 'w';	break;
	case Tyuint64:	return 'l';	break;
	case Tyflt32:	return 's';	break;
	case Tyflt64:	return 'd';	break;
	case Typtr:	return 'l';	break;
	default:	return 'l';	break;
	}
}

char *qtype(Gen *g, Type *ty)
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
	case Tyslice:	return "l, l";	break;
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

void putloc(FILE *f, Loc l)
{
	char buf[1024];
	char sigil;
	Node *dcl;

	switch (l.kind) {
	case Ldecl:
		dcl = decls[l.dcl];
		sigil = dcl->decl.isglobl ? '$' : '%';
		asmname(buf, sizeof buf, dcl->decl.name, sigil);
		fprintf(f, "%s", buf);
		break;
	case Ltemp:	fprintf(f, "%%%lld", (vlong)l.tmp);	break;
	case Lconst:	fprintf(f, "%lld", (vlong)l.cst);	break;
	case Llabel:	fprintf(f, "@%s", l.lbl);		break;
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
		case 's':	fprintf(g->file, "%s", va_arg(ap, char*));	break;
		case 'v':	putloc(g->file, va_arg(ap, Loc));		break;
		case 'd':	fprintf(g->file, "%d", va_arg(ap, int));	break;
		case 'l':	fprintf(g->file, "%lld", va_arg(ap, vlong));	break;
		case 't':	fprintf(g->file, "%c", va_arg(ap, int));	break;
		case 'T':	
			t = va_arg(ap, Type*);
			fputs(qtype(g, t), g->file);
			break;
		default:	
			die("bad format character %c", *p);
			break;
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

static Loc binop(Gen *g, char *op, Node *a, Node *b)
{
	Loc t, l, r;
	char tag;

	tag = qtag(g, exprtype(a));
	t = qtemp(g, tag);
	l = rval(g, a);
	r = rval(g, b);
	out(g, "\t%v =%t %s %v, %v\n", t, tag, op, l, r);
	return t;
}

static Loc binopk(Gen *g, char *op, Node *n, uvlong r)
{
	Loc t, l;
	char tag;

	tag = qtag(g, exprtype(n));
	t = qtemp(g, tag);
	l = rval(g, n);
	out(g, "\t%v =%t %s %v, %l\n", t, tag, op, l, r);
	return t;
}

/*static*/ Loc getcode(Gen *g, Node *n)
{
	Loc r, p;

	if (isconstfn(n))
		r = qvar(g, decls[n->expr.did]);
	else {
		r = qtemp(g, 'l');
		p = binopk(g, "add", n, Ptrsz);
		out(g, "\t%v =l loadl %v", r, p);
	}
	return r;
}

static Loc slicebase(Gen *g, Node *slnode, Node *offnode)
{
	Loc u, v, r;
	Type *ty;
	size_t sz;
	Loc sl, off;

	ty = tybase(exprtype(slnode));
	sl = rval(g, slnode);
	switch (ty->type) {
	case Typtr:	u = sl;	break;
	case Tyarray:	u = sl;	break;
	case Tyslice:	
		u = qtemp(g, 'l');
		out(g, "\t%v =l loadl %v", u, sl); break;
	default: die("Unslicable type %s", tystr(ty));
	}
	/* safe: all types we allow here have a sub[0] that we want to grab */
	if (!offnode)
		return u;
	off = ptrsized(g, rval(g, offnode));
	sz = tysize(slnode->expr.type->sub[0]);
	v = qtemp(g, 'l');
	out(g, "\t%v =l mull %v, %l\n", v, off, sz);
	r = qtemp(g, 'l');
	out(g, "\t%v =l add %v, %v\n", r, off, sz);
	return r;
}

static Loc slicelen(Gen *g, Loc sl)
{
	Loc szp, r;

	szp = qtemp(g, 'l');
	out(g, "\t%v =l add %v, %l\n", szp, sl, Ptrsz);
	r = qtemp(g, 'l');
	out(g, "\t%v =l loadl %v", r, szp);
	return r;
}

Loc cmpop(Gen *g, char *op, Node *ln, Node *rn)
{
	Loc l, r, t;
	char tag;

	tag = qtag(g, exprtype(ln));
	t = qtemp(g, tag);
	l = rval(g, ln);
	r = rval(g, rn);
	out(g, "\t%v =w c%s%c %v, %v\n", t, op, tag, l, r);
	return t;
}

static Loc intcvt(Gen *g, Loc val, char to, char from, int signconv)
{
	char sign;
	Loc t;

	sign = 'u';
	if (signconv)
		sign = 's';

	t = qtemp(g, to);
	if (qtagsizes[to & 0x7f] > qtagsizes[from & 0x7f])
		out(g, "\t%v =%t ext%s%c %v", t, to, sign, from, val);
	else
		out(g, "\t%v =%t copy %v", t, to, val);
	return t;
}

static Loc gencast(Gen *g, Srcloc loc, Loc val, Type *to, Type *from)
{
	Loc a, r;

	r = val;
	/* do the type conversion */
	switch (tybase(to)->type) {
	case Tybool:
	case Tyint8: case Tyint16: case Tyint32: case Tyint64:
	case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
	case Tyint: case Tyuint: case Tychar: case Tybyte:
	case Typtr:
		switch (from->type) {
		/* ptr -> slice conversion is disallowed */
		case Tyslice:
			if (tybase(to)->type != Typtr)
				lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
			r = qtemp(g, 'l');
			out(g, "\t%v =l loadl %v", r, val);
			break;
		case Tyfunc:
			if (to->type != Typtr)
				lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
			a = qtemp(g, 'l');
			r = qtemp(g, 'l');
			out(g, "\t%v =l addl %v, %l", a, val, Ptrsz);
			out(g, "\t%v =l loadl %v", r, a);
			break;
		/* signed conversions */
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyint:
			r = intcvt(g, val, qtag(g, to), qtag(g, from), 1);
			break;
		/* unsigned conversions */
		case Tybool:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyuint: case Tychar: case Tybyte:
		case Typtr:
			r = intcvt(g, val, qtag(g, to), qtag(g, from), 0);
			break;
		case Tyflt32: case Tyflt64:
			if (tybase(to)->type == Typtr)
				lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
			die("unimplemented cast type");
			//r = mkexpr(loc, Oflt2int, rval(g, val, NULL), NULL);
			//r->expr.type = to;
			break;
		default:
			lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
		}
		break;
	case Tyflt32: case Tyflt64:
		switch (from->type) {
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyint: case Tyuint: case Tychar: case Tybyte:
			die("unimplemented cast type");
			//r = mkexpr(loc, Oint2flt, rval(g, val, NULL), NULL);
			//r->expr.type = to;
			break;
		case Tyflt32: case Tyflt64:
			die("unimplemented cast type");
			//r = mkexpr(val->loc, Oflt2flt, rval(g, val, NULL), NULL);
			//r->expr.type = to;
			break;
		default:
			lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
			break;
		}
		break;
		/* no other destination types are handled as things stand */
	default:
		lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
	}
	return r;
}

static Loc simpcast(Gen *g, Node *val, Type *to)
{
	Loc l;

	l = rval(g, val);
	return gencast(g, val->loc, l, to, exprtype(val->expr.args[0]));
}


void blit(Gen *g, Loc dst, Loc src, size_t sz)
{
	out(g, "\tblit %v, %v, %l\n", dst, src, sz);
}

Loc assign(Gen *g, Node *dst, Node* src)
{
	Loc d, s;

	d = lval(g, dst);
	s = rval(g, src);
	if (src != dst) {
		if (isstacktype(exprtype(dst)))
			blit(g, d, s, size(dst));
		else
			out(g, "\t%v =%t copy %v\n", dst, exprtype(dst), src);
	}
	return d;
}

/*static*/ void checkidx(Gen *g, char *op, Node *len, Node *idx)
{
	char ok[128], fail[128];
	Loc inrange;

	if (!len)
		return;
	genlblstr(ok, sizeof ok, "");
	genlblstr(fail, sizeof fail, "");
	inrange = binop(g, op, len, idx);

	out(g, "\tjnz %v, %s, %s\n", inrange, ok, fail);
	out(g, "@%s", fail);
	out(g, "\tcall %_rt$abort_oob()\n");
	out(g, "@%s", ok);
}

static Loc lval(Gen *g, Node *n)
{
	Node **args;
	Loc r;

	args = n->expr.args;
	switch (exprop(n)) {
	case Ovar:	r = qvar(g, decls[n->expr.did]);	break;
	case Oidx:	r = loadidx(g, args[0], args[1]);	break;
	//case Oderef:	r = deref(rval(s, args[0], NULL), NULL);	break;
	case Omemb:	r = rval(g, n);	break;
	case Ostruct:	r = rval(g, n);	break;
	case Oucon:	r = rval(g, n);	break;
	case Oarr:	r = rval(g, n);	break;
	case Ogap:	r = temp(g, n);	break;

	/* not actually expressible as lvalues in syntax, but we generate them */
	case Oudata:	r = rval(g, n);	break;
	case Outag:	r = rval(g, n);	break;
	case Otupget:	r = rval(g, n);	break;
	default:
			fatal(n, "%s cannot be an lvalue", opstr[exprop(n)]);
			break;
	}
	return r;
}

static Loc gencall(Gen *g, Node *n)
{
	Loc ret, fn;
	Loc *args;
	Type **types, *ty;
	size_t nargs, i;
	int hasret;
	char *sep;

	args = malloc(n->expr.nargs * sizeof(Loc));
	types = malloc(n->expr.nargs * sizeof(Type *));

	ret = Z;
	hasret = 0;
	if (tybase(exprtype(n))->type != Tyvoid) {
		ret = temp(g, n);
		hasret = 1;
	}

	nargs = 0;
	for (i = 1; i < n->expr.nargs; i++) {
		ty = exprtype(n->expr.args[i]);
		ty = tybase(ty);
		if (ty->type == Tyvoid)
			continue;
		args[nargs] = rval(g, n->expr.args[i]);
		types[nargs] = ty;
	}
	fn = rval(g, n->expr.args[0]);

	if (hasret)
		out(g, "\t%v =%t call %v (", ret, exprtype(n), fn);
	else
		out(g, "\tcall %v (", fn);

	sep = "";
	for (i = 1; i < nargs; i++) {
		out(g, "%s%t %v", sep, types[i], args[i]);
		sep = ", ";
	}
	out(g, ")\n");
	free(args);
	return ret;
}

static Loc genslice(Gen *g, Node *sl)
{
	die("genslize not implemented\n");
}

int stacknode(Node *n)
{
	if (n->type == Nexpr)
		return isstacktype(n->expr.type);
	else
		return isstacktype(n->decl.type);
}

static Loc loadidx(Gen *g, Node *base, Node *idx)
{
	die("loadidx not implemented\n");
}

static char *strlabel(Gen *g, Node *str) {
	die("strval not implemented\n");
}

static Loc loadlit(Gen *g, Node *n)
{
	n = n->expr.args[0];
	switch (n->lit.littype) {
	case Lchr:	return qconst(g, n->lit.chrval, 'w');	break;
	case Lbool:	return qconst(g, n->lit.boolval, 'w');	break;
	case Lint:	return qconst(g, n->lit.intval, 'w');	break;
	case Lflt:	return qconst(g, n->lit.fltval, 'w');	break;
	case Lstr:	return qlabel(g, strlabel(g, n), 'l');	break;
	case Llbl:	return qlabel(g, n->lit.lblval, 'l');	break;
	case Lvoid:	return qconst(g, n->lit.chrval, 'w');	break;
	case Lfunc:	die("func literals not implemented\n");	break;
	}
}

Loc rval(Gen *g, Node *n)
{
	Loc r; /* expression result */
	Node **args;
	Type *ty;

	r = Z;
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

	case Oslbase:	r = slicebase(g, args[0], args[1]);	break;
	case Osllen:	r = slicelen(g, rval(g, args[0]));	break;
	case Oslice:	r = genslice(g, n);			break;
	case Oidx:	r = loadidx(g, args[0], args[1]);	break;

	case Ojmp:	out(g, "\tjmp %v\n", args[0]);		break;
	case Ocast:	r = simpcast(g, args[0], exprtype(n));	break;
	case Ocall:	r = gencall(g, n);			break;

	case Ovar:	r = qvar(g, decls[n->expr.did]);	break;
	case Olit:	r = loadlit(g, n);			break;
	case Osize:	r = qconst(g, size(args[0]), 'l');	break;
	case Oret:
		ty = tybase(exprtype(args[0]));
		if (ty->type != Tyvoid)
			assign(g, g->retval, args[0]);
		out(g, "\tjmp %v\n", g->retlbl);
		break;

	case Oderef:
		ty = tybase(exprtype(args[0]));
		if (isstacktype(ty)) {
			r = rval(g, args[0]);
		} else {
			r = temp(g, args[0]);
			out(g, "\t%v =%t load%t %v\n", r, ty, ty, args[0]);
		}
		break;
	case Oaddr:
		ty = tybase(exprtype(args[0]));
		if (isstacktype(ty)) {
			r = rval(g, args[0]);
		} else {
			die("taking address of non-addressable value");
		}
		break;

	case Ocjmp:
		out(g, "\tjnz %v, %v, %v\n", rval(g, args[0]), args[1], args[2]);
		break;

	case Omemb:
	case Oucon:
	case Outag:
	case Oudata:
	case Otup:
	case Oarr:
	case Ostruct:
	case Ogap:
	case Oneg:
	case Otupget:
	case Olnot:
	case Ovjmp:
	case Oset:
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
			break;
		case Nexpr:
			rval(g, bb->nl[i]);
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

void genlocals(Gen *g, Node **locals, size_t nlocals)
{
	Type *ty;
	size_t i;

	for (i = 0; i < nlocals; i++) {
		ty = decltype(locals[i]);
		pr(g, "\t%%%s =l alloc%zd %zd\n", declname(locals[i]), tyalign(ty), tysize(ty));
	}
}

void genfn(Gen *g, Node *dcl, Node **locals, size_t nlocals)
{
	Node *n, *b, *retdcl;
	size_t i;
	Cfg *cfg;
	Bb *bb;
	Type *ret;
	char name[1024];

	if (dcl->decl.isextern || dcl->decl.isgeneric)
		return;

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
			pr(g, "%c ", qtag(g, ret));
		else
			pr(g, "%c ", qtype(g, ret));
	}
	pr(g, "%s", name);
	funcargs(g, n);

	/* func body */
	pr(g, "{\n");
	pr(g, "@start\n");
	genlocals(g, locals, nlocals);
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
		pr(g, "\tb %zd,\n", val);
		return;
	}

	for (i = 1; i < 8; i++)
		if (val < 1ULL << (7*i))
			break;
	shift = 8 - i;
	b = ~0ull << (shift + 1);
	b |= val & ~(~0ull << shift);
	pr(g, "\tb %u,\n", b);
	val >>=  shift;
	while (val != 0) {
		pr(g, "\tb %u,\n", (uint)val & 0xff);
		val >>= 8;
	}
}

static void outbytes(Gen *g, char *p, size_t sz)
{
	size_t i;

	for (i = 0; i < sz; i++) {
		if (i % 60 == 0)
			pr(g, "\tb \"");
		if (p[i] == '"' || p[i] == '\\')
			pr(g, "\\");
		if (isprint(p[i]))
			pr(g, "%c", p[i]);
		else
			pr(g, "\\%03o", (uint8_t)p[i] & 0xff);
		/* line wrapping for readability */
		if (i % 60 == 59 || i == sz - 1)
			pr(g, "\",\n");
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
		pr(g, "data $%s = {\n", b->lbl);
	}

	switch (b->type) {
	case Btimin:	encodemin(g, b->ival);	break;
	case Bti8:	pr(g, "\tb %zd,\n", b->ival);	break;
	case Bti16:	pr(g, "\th %zd,\n", b->ival);	break;
	case Bti32:	pr(g, "\tw %zd,\n", b->ival);	break;
	case Bti64:	pr(g, "\tl %zd,\n", b->ival);	break;
	case Btbytes:	outbytes(g, b->bytes.buf, b->bytes.len);	break;
	case Btpad:	pr(g, "\tz %zd,\n", b->npad);	break;
	case Btref:	pr(g, "\tl $%s + %zd,\n", b->ref.str, b->ref.off);	break;
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

void gentydesc(Gen *g)
{
	Blob *b;
	Type *ty;
	size_t i;

	for (i = Ntypes; i < ntypes; i++) {
		if (!types[i]->isreflect)
			continue;
		ty = tydedup(types[i]);
		if (ty->isemitted || ty->isimport)
			continue;
		ty->isemitted = 1;
		b = tydescblob(ty);
		b->iscomdat = 1;
		genblob(g, b);
		blobfree(b);
	}
	pr(g, "\n");
}

void outarray(Gen *g, Type *ty)
{
	size_t sz;

	sz = 0;
	if (ty->asize)
		sz = ty->asize->expr.args[0]->lit.intval;
	outtypebody(g, ty->sub[0]);
	pr(g, "\t%s %zd,\n", qtype(g, ty->sub[0]), sz);
}

void outstruct(Gen *g, Type *ty)
{
	size_t i;
	Type *mty;

	for (i = 0; i < ty->nmemb; i++) {
		mty = decltype(ty->sdecls[i]);
		outtypebody(g, mty);
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
	pr(g, "%zd\n", maxsize);
}

void outtuple(Gen *g, Type *ty)
{
	size_t i;
	Type *mty;

	for (i = 0; i < ty->nsub; i++) {
		mty = ty->sub[i];
		outtypebody(g, mty);
		pr(g, "\t%s,\n", qtype(g, mty));
	}
}

void outtypebody(Gen *g, Type *ty)
{
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
	case Tyname:	pr(g, "\t:t%zd,\n", ty->tid);	break;

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

static void outstructtype(Gen *g, Type *ty)
{
	size_t i;

	for (i = 0; i < ty->nmemb; i++)
		outqtype(g, decltype(ty->sdecls[i]));
}

static void outtupletype(Gen *g, Type *ty)
{
	size_t i;

	for (i = 0; i < ty->nsub; i++)
		outqtype(g, ty->sub[i]);
}


static void outuniontype(Gen *g, Type *ty)
{
	size_t i;
	Type *mty;

	for (i = 0; i < ty->nmemb; i++) {
		mty = ty->udecls[i]->etype;
		if (!mty)
			continue;
		outqtype(g, mty);
	}
}

void outqtype(Gen *g, Type *ty)
{
	char buf[1024];
	Ty tt;

	ty = tydedup(ty);
	tt = ty->type;
	if (tt == Tycode || tt == Tyvar || tt == Tyunres || hasparams(ty))
		return;
	if (g->typenames[ty->tid])
		return;
	if (ty->vis == Visbuiltin)
		return;

	snprintf(buf, sizeof buf, ":t%d", ty->tid);
	g->typenames[ty->tid] = strdup(buf);

	switch (tt) {
	case Tyarray:	outqtype(g, ty->sub[0]);	break;
	case Tystruct:	outstructtype(g, ty);		break;
	case Tytuple:	outtupletype(g, ty);		break;
	case Tyunion:	outuniontype(g, ty);		break;
	case Tyname:	outqtype(g, ty->sub[0]);	break;
	default:
		break;
	}

	pr(g, "type %s = align %zd {\n", g->typenames[ty->tid], tyalign(ty));
	if (tt != Tyname)
		outtypebody(g, ty);
	else
		outtypebody(g, ty->sub[0]);
	pr(g, "}\n\n");
}

void genqtypes(Gen *g)
{
	size_t i;

	g->typenames = zalloc(ntypes * sizeof(Type *));
	for (i = Ntypes; i < ntypes; i++) {
		outqtype(g, types[i]);
	}
}

void gen(Node *file, char *out)
{
	Htab *globls;
	Node **locals;
	size_t nlocals;
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
	g->stkoff = mkht(varhash, vareq);
	g->envoff = mkht(varhash, vareq);
	g->globls = mkht(varhash, vareq);

	genqtypes(g);
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
				n = flattenfn(n, &locals, &nlocals);
				genfn(g, n, locals, nlocals);
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
