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


struct Gen {
	FILE *f;
	Cfg *cfg;
	char **typenames;
	Htab *strtab;

	/* memory offsets/addrs */
	Htab *envoff;
	Htab *stkoff;
	Htab *globls;
	size_t stksz;

	Node **local;
	size_t nlocal;
	size_t nexttmp;

	Type *rettype;
	Node *retval;
	Loc *arg;
	size_t narg;
	Loc retlbl;

	Insn *insn;
	size_t ninsn;
	size_t insnsz;
};

char qtag(Gen *g, Type *ty);
static void outtypebody(Gen *g, Type *ty);
static void outqtype(Gen *g, Type *ty);

static void o(Gen *g, Qop o, Loc r, Loc a, Loc b);
static void ocall(Gen *g, Loc fn, Loc ret, Loc env, Loc *args, Type **types, size_t nargs);
static Loc rval(Gen *g, Node *n);
static Loc lval(Gen *g, Node *n);
static Loc seqbase(Gen *g, Node *sl, Node *off);
static Loc slicelen(Gen *g, Loc sl);
static Loc gencast(Gen *g, Srcloc loc, Loc val, Type *to, Type *from);

static Loc abortoob;
static Loc ptrsize;

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

int isstack(Node *n)
{
	if (n->type == Nexpr)
		return isstacktype(n->expr.type);
	else
		return isstacktype(n->decl.type);
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
	bprintf(buf, nbuf, "%c%s%s%s", sigil, ns, sep, name);
	return buf;
}


static Loc qtemp(Gen *g, char type)
{
	return (Loc){.kind=Ltemp, .tmp=g->nexttmp++, .tag=type};
}

static Loc temp(Gen *g, Node *n)
{
	char type;

	if (n->type == Ndecl)
		type = qtag(g, decltype(n));
	else
		type = qtag(g, exprtype(n));
	return (Loc){.kind=Ltemp, .tmp=g->nexttmp++, .tag=type};
}

static Loc qvar(Gen *g, Node *n)
{
	char tag;

	assert(n);
	if (n->type == Nexpr && exprop(n) == Ovar)
		n = decls[n->expr.did];
	assert(n->type == Ndecl);
	tag = qtag(g, n->decl.type);
	return (Loc){.kind=Ldecl, .dcl=n->decl.did, .tag=tag};
}

static Loc qlabelstr(Gen *g, char *lbl)
{
	return (Loc){.kind=Llabel, .lbl=lbl, .tag='l'};
}

static Loc qlabel(Gen *g, Node *lbl)
{
	return (Loc){.kind=Llabel, .lbl=lblstr(lbl), .tag='l'};
}

static Loc qconst(Gen *g, uint64_t cst, char tag)
{
	return (Loc){.kind=Lconst, .cst=cst, .tag=tag};
}

static void o(Gen *g, Qop op, Loc r, Loc a, Loc b)
{
	if (op == Qcsltl)
		assert(a.tag == b.tag);
	if (g->ninsn == g->insnsz) {
		g->insnsz += g->insnsz/2 + 1;
		g->insn = xrealloc(g->insn, g->insnsz * sizeof(Insn));
	}
	g->insn[g->ninsn].op = op;
	g->insn[g->ninsn].ret = r;
	g->insn[g->ninsn].arg[0] = a;
	g->insn[g->ninsn].arg[1] = b;
	g->ninsn++;
}

static void ocall(Gen *g, Loc fn, Loc ret, Loc env, Loc *args, Type **types, size_t nargs) 
{
	if (g->ninsn == g->insnsz) {
		g->insnsz += g->insnsz/2 + 1;
		g->insn = xrealloc(g->insn, g->insnsz * sizeof(Insn));
	}
	g->insn[g->ninsn].op = Qcall;
	g->insn[g->ninsn].fn = fn;
	g->insn[g->ninsn].ret = ret;
	g->insn[g->ninsn].env = env;
	g->insn[g->ninsn].farg = args;
	g->insn[g->ninsn].fty = types;
	g->insn[g->ninsn].nfarg = nargs;
	g->ninsn++;
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

static void addlocal(Gen *g, Node *n)
{
	lappend(&g->local, &g->nlocal, n);
}

static Loc qstacktmp(Gen *g, Node *e)
{
	Node *dcl;

	assert(e->type == Nexpr);
	gentemp(e->loc, e->expr.type, &dcl);
	addlocal(g, dcl);
	return qvar(g, dcl); 
}

static Loc ptrsized(Gen *g, Loc v)
{
	Loc r;

	if (v.tag == 'l')
		return v;
	r = qtemp(g, 'l');
	switch (v.tag) {
	case 'w':	o(g, Qextuw, r, v, Zq);	break;
	case 'h':	o(g, Qextuh, r, v, Zq);	break;
	case 'b':	o(g, Qextub, r, v, Zq);	break;
	default:	die("invalid tag\n");	break;
	}
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
	default:	return g->typenames[ty->tid];	break;
	}
}

static Qop loadop(Type *ty)
{
	Qop op;

	switch (tybase(ty)->type) {
	case Tybool:	op = Qloadub;	break;
	case Tybyte:	op = Qloadub;	break;
	case Tyuint8:	op = Qloadub;	break;
	case Tyint8:	op = Qloadsb;	break;

	case Tyint16:	op = Qloadsh;	break;
	case Tyuint16:	op = Qloaduh;	break;

	case Tyint:	op = Qloadsw;	break;
	case Tyint32:	op = Qloadsw;	break;
	case Tychar:	op = Qloaduw;	break;
	case Tyuint32:	op = Qloaduw;	break;

	case Tyint64:	op = Qloadl;	break;
	case Tyuint64:	op = Qloadl;	break;
	case Typtr:	op = Qloadl;	break;
	case Tyflt32:	op = Qloads;	break;
	case Tyflt64:	op = Qloadd;	break;
	default:	die("badload");	break;
	}
	return op;
}

static Qop storeop(Type *ty)
{
	Qop op;

	switch (tybase(ty)->type) {
	case Tybool:	op = Qstoreb;	break;
	case Tybyte:	op = Qstoreb;	break;
	case Tyuint8:	op = Qstoreb;	break;
	case Tyint8:	op = Qstoreb;	break;

	case Tyint16:	op = Qstoreh;	break;
	case Tyuint16:	op = Qstoreh;	break;

	case Tyint:	op = Qstorew;	break;
	case Tyint32:	op = Qstorew;	break;
	case Tychar:	op = Qstorew;	break;
	case Tyuint32:	op = Qstorew;	break;

	case Tyint64:	op = Qstorel;	break;
	case Tyuint64:	op = Qstorel;	break;
	case Typtr:	op = Qstorel;	break;
	case Tyflt32:	op = Qstores;	break;
	case Tyflt64:	op = Qstored;	break;
	default:	die("badstore");	break;
	}
	return op;
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

static void initconsts(Gen *g, Htab *globls)
{
	Type *ty;
	Node *name;
	Node *dcl;
	char buf[1024];

	ptrsize = qconst(g, Ptrsz, 'l');
	ty = mktyfunc(Zloc, NULL, 0, mktype(Zloc, Tyvoid));
	ty->type = Tycode;
	name = mknsname(Zloc, "_rt", "abort_oob");
	dcl = mkdecl(Zloc, name, ty);
	dcl->decl.isconst = 1;
	dcl->decl.isextern = 1;
	dcl->decl.isglobl = 1;
	asmname(buf, sizeof buf, dcl->decl.name, '$');
	htput(globls, dcl, strdup(buf));

	abortoob = qvar(g, dcl);
}

static Loc binop(Gen *g, Qop op, Node *a, Node *b)
{
	Loc t, l, r;
	char tag;

	tag = qtag(g, exprtype(a));
	t = qtemp(g, tag);
	l = rval(g, a);
	r = rval(g, b);
	o(g, op, t, l, r);
	return t;
}

static Loc binopk(Gen *g, Qop op, Node *n, uvlong k)
{
	Loc t, l, r;
	char tag;

	tag = qtag(g, exprtype(n));
	t = qtemp(g, tag);
	l = rval(g, n);
	r = qconst(g, k, l.tag);
	o(g, op, t, l, r);
	return t;
}

/*static*/ Loc getcode(Gen *g, Node *n)
{
	Loc r, p;

	if (isconstfn(n))
		r = qvar(g, n);
	else {
		r = qtemp(g, 'l');
		p = binopk(g, Qadd, n, Ptrsz);
		o(g, Qloadl, r, p, Zq);
	}
	return r;
}

static Loc slicelen(Gen *g, Loc sl)
{
	Loc lp, r;

	lp = qtemp(g, 'l');
	r = qtemp(g, 'l');
	o(g, Qadd, lp, sl, ptrsize);
	o(g, Qloadl, r, lp, Zq);
	return r;
}

Loc cmpop(Gen *g, Op op, Node *ln, Node *rn)
{
	Qop intcmptab[][2][2] = {
		[Ole] = {{Qcslew, Qculew}, {Qcslel, Qculel}},
		[Olt] = {{Qcsltw, Qcultw}, {Qcsltl, Qcultl}},
		[Ogt] = {{Qcsgtw, Qcugtw}, {Qcsgtl, Qcugtl}},
		[Oge] = {{Qcsgew, Qcugew}, {Qcsgel, Qcugel}},
		[Oeq] = {{Qceqw, Qceqw}, {Qceql, Qceql}},
		[One] = {{Qcnew, Qcnew}, {Qcnel, Qcnel}},
	};

	Qop fltcmptab[][2] = {
		[Ofle] = {Qcles, Qcled},
		[Oflt] = {Qclts,  Qcltd},
		[Ofgt] = {Qcgts,  Qcgtd},
		[Ofge] = {Qcges,  Qcged},
		[Ofeq] = {Qceqs,  Qceqd},
		[Ofne] = {Qcnes,  Qcned},
	};

	Loc l, r, t;
	char tag;
	Type *ty;
	Qop qop;

	ty = exprtype(ln);
	tag = qtag(g, ty);
	if (istyfloat(ty))
		qop = fltcmptab[op][tag == 'd'];
	else
		qop = intcmptab[op][istysigned(ty)][tag == 'l'];

	t = qtemp(g, tag);
	l = rval(g, ln);
	r = rval(g, rn);
	o(g, qop, t, l, r);
	return t;
}

static Loc intcvt(Gen *g, Loc val, char to, int sz, int sign)
{
	Loc t;
	Qop optab[][2] = {
		[8] = {Qcopy,	Qcopy},
		[4] = {Qextuw,	Qextsw},
		[2] = {Qextuh,	Qextsh},
		[1] = {Qextub,	Qextsb},
	};

	t = qtemp(g, to);
	o(g, optab[sz][sign], t, val, Zq);
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
		switch (tybase(from)->type) {
		/* ptr -> slice conversion is disallowed */
		case Tyslice:
			/* FIXME: we should only allow casting to pointers. */
			if (tysize(to) != Ptrsz)
				lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
			r = qtemp(g, 'l');
			o(g, Qloadl, r, val, Zq);
			break;
		case Tyfunc:
			if (to->type != Typtr)
				lfatal(loc, "bad cast from %s to %s", tystr(from), tystr(to));
			a = qtemp(g, 'l');
			r = qtemp(g, 'l');
			o(g, Qadd, a, val, ptrsize);
			o(g, Qloadl, r, a, Zq);
			break;
		/* signed conversions */
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyint:
			r = intcvt(g, val, qtag(g, to), tysize(from), 1);
			break;
		/* unsigned conversions */
		case Tybool:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyuint: case Tychar: case Tybyte:
		case Typtr:
			r = intcvt(g, val, qtag(g, to), tysize(from), 0);
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
	return gencast(g, val->loc, l, to, exprtype(val));
}


void blit(Gen *g, Loc dst, Loc src, size_t sz, size_t align)
{
	static const Qop ops[][2] = {
		[8] = {Qloadl, Qstorel},
		[4] = {Qloaduw, Qstorew},
		[2] = {Qloaduh, Qstoreh},
		[1] = {Qloadub, Qstoreb},
	};
	Loc l, a;
	size_t i;

	assert(sz % align == 0);
	l = qtemp(g, 'l');
	a = qconst(g, align, 'l');
	for (i = 0; i < sz; i += align) {
		o(g, ops[align][0], l, src, Zq);
		o(g, ops[align][1], Zq, l, dst);
		o(g, Qadd, src, src, a);
		o(g, Qadd, dst, dst, a);
	}
}

Loc assign(Gen *g, Node *dst, Node* src)
{
	Loc d, s;
	Type *ty;

	ty = exprtype(dst);
	d = lval(g, dst);
	s = rval(g, src);
	if (isstacktype(ty))
		blit(g, d, s, tysize(ty), tyalign(ty));
	else 
		o(g, storeop(ty), Zq, s, d);
	return d;
}

/*static*/ void checkidx(Gen *g, Qop op, Loc len, Loc idx)
{
	char ok[128], fail[128];
	Loc oklbl, faillbl;
	Loc inrange;

	genlblstr(ok, sizeof ok, "");
	genlblstr(fail, sizeof fail, "");

	inrange = qtemp(g, 'w');
	oklbl = qlabelstr(g, strdup(fail));
	faillbl = qlabelstr(g, strdup(fail));

	o(g, op, inrange, len, idx);
	o(g, Qjnz, inrange, oklbl, faillbl);
	o(g, Qlabel, faillbl, Zq, Zq);
	ocall(g, abortoob, Zq, Zq, NULL, NULL, 0);
	o(g, Qlabel, oklbl, Zq, Zq);
}

static Loc loadvar(Gen *g, Node *var)
{
	Node *dcl;
	Type *ty;
	Loc r, l;

	dcl = decls[var->expr.did];
	ty = exprtype(var);
	l = qvar(g, dcl);
	if (isstacktype(ty)) {
		r = l;
	} else {
		r = qtemp(g, l.tag);
		o(g, loadop(ty), r, l, Zq);
	}
	return r;
}

static Loc deref(Gen *g, Node *n)
{
	Loc l, r;
	Type *ty;

	ty = exprtype(n);
	l = rval(g, n);
	if (isstacktype(ty))
		return l;
	r = qtemp(g, qtag(g, ty));
	o(g, loadop(ty), r, l, Zq);
	return r;
}

static Loc lval(Gen *g, Node *n)
{
	Node **args;
	Loc r;

	args = n->expr.args;
	switch (exprop(n)) {
	case Oidx:	r = seqbase(g, args[0], args[1]);	break;
	case Oderef:	r = deref(g, args[0]);	break;
	case Ovar:	r = qvar(g, n);	break;
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
	Loc env, ret, fn;
	Loc *args;
	Type **types, *ty;
	size_t nargs, i;

	args = malloc(n->expr.nargs * sizeof(Loc));
	types = malloc(n->expr.nargs * sizeof(Type *));

	ret = Zq;
	env = Zq;
	if (tybase(exprtype(n))->type != Tyvoid)
		ret = temp(g, n);

	nargs = 0;
	for (i = 1; i < n->expr.nargs; i++) {
		ty = exprtype(n->expr.args[i]);
		ty = tybase(ty);
		if (ty->type == Tyvoid)
			continue;
		args[nargs] = rval(g, n->expr.args[i]);
		types[nargs] = ty;
		nargs++;
	}
	fn = rval(g, n->expr.args[0]);

	ocall(g, fn, ret, env, args, types, nargs);
	return ret;
}

static void checkbounds(Gen *g, Node *n, Loc len)
{
}

static Loc seqbase(Gen *g, Node *slnode, Node *offnode)
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
		o(g, Qloadl, u, sl, Zq);
		break;
	default: 
		die("Unslicable type %s", tystr(ty));
	}
	/* safe: all types we allow here have a sub[0] that we want to grab */
	if (!offnode)
		return u;
	off = ptrsized(g, rval(g, offnode));
	sz = tysize(slnode->expr.type->sub[0]);
	v = qtemp(g, 'l');
	r = qtemp(g, 'l');
	o(g, Qmul, v, off, qconst(g, sz, 'l'));
	o(g, Qadd, r, u, v);
	return r;
}

static Loc genslice(Gen *g, Node *n)
{
	Node *arg, *off, *lim;
	Loc lo, hi, start, sz;
	Loc base, len;
	Loc sl, lenp;
	Type *ty;


	arg = n->expr.args[0];
	off = n->expr.args[1];
	lim = n->expr.args[1];
	ty = exprtype(arg);
	sl = qstacktmp(g, n);
	lenp = qtemp(g, 'l');
	o(g, Qadd, lenp, sl, ptrsize);
	/* Special case: Literal arrays with 0 need to be
	have zero base pointers and lengths in order to 
	act as nice initializers */
	if (exprop(arg) == Oarr && arg->expr.nargs == 0) {
		base = qconst(g, 0, 'l');
	} else {
		base = seqbase(g, arg, off);
	}
	lo = rval(g, off);
	hi = rval(g, lim);
	sz = qconst(g, tysize(ty->sub[0]), 'l');
	len = qtemp(g, 'l');
	start = qtemp(g, 'l');

	o(g, Qadd, base, base, lo);
	o(g, Qsub, len, hi, lo);
	o(g, Qmul, start, len, sz);

	checkbounds(g, n, len);
	o(g, Qstorel, Zq, base, sl);
	o(g, Qstorel, Zq, len, lenp);
	return sl;
}

static Loc genucon(Gen *g, Node *n)
{
	Loc u, tag, dst, elt, align;
	size_t sz, al;
	Type *ty;
	Ucon *uc;

	ty = tybase(exprtype(n));
	uc = finducon(ty, n->expr.args[0]);
	if (!uc)
		die("Couldn't find union constructor");

	u = qstacktmp(g, n);
	tag = qconst(g, uc->id, 'w');
	o(g, Qstorew, Zq, tag, u);
	if (!uc->etype)
		return u;

	al = max(4, tyalign(uc->etype));
	sz = tysize(uc->etype);
	dst = qtemp(g, 'l');
	align = qconst(g, al, 'l');
	o(g, Qadd, dst, u, align);

	elt = rval(g, n->expr.args[1]);
	if (isstacktype(uc->etype))
		blit(g, dst, elt, sz, al);
	else
		o(g, storeop(uc->etype), Zq, elt, dst);
	return u;
}

static Loc strlabel(Gen *g, Node *str) {
	die("strval not implemented\n");
}

static Loc loadlit(Gen *g, Node *n)
{
	n = n->expr.args[0];
	switch (n->lit.littype) {
	case Llbl:	return qlabel(g, n);			break;
	case Lstr:	return strlabel(g, n);			break;
	case Lchr:	return qconst(g, n->lit.chrval, 'w');	break;
	case Lbool:	return qconst(g, n->lit.boolval, 'w');	break;
	case Lint:	return qconst(g, n->lit.intval, 'w');	break;
	case Lflt:	return qconst(g, n->lit.fltval, 'w');	break;
	case Lvoid:	return qconst(g, n->lit.chrval, 'w');	break;
	case Lfunc:	die("func literals not implemented\n");	break;
	}
}

Loc rval(Gen *g, Node *n)
{
	Node **args;
	Type *ty;
	Loc r, l;
	Op op;

	r = Zq;
	args = n->expr.args;
	op = exprop(n);
	switch (op) {
	/* arithmetic */
	case Oadd:	r = binop(g, Qadd, args[0], args[1]);	break;
	case Osub:	r = binop(g, Qsub, args[0], args[1]);	break;
	case Omul:	r = binop(g, Qmul, args[0], args[1]);	break;
	case Odiv:	r = binop(g, Qdiv, args[0], args[1]);	break;
	case Omod:	r = binop(g, Qrem, args[0], args[1]);	break;
	case Ofadd:	r = binop(g, Qadd, args[0], args[1]);	break;
	case Ofsub:	r = binop(g, Qsub, args[0], args[1]);	break;
	case Ofmul:	r = binop(g, Qmul, args[0], args[1]);	break;
	case Ofdiv:	r = binop(g, Qdiv, args[0], args[1]);	break;
	//case Ofneg:	r = binop(g, Qneg, args[0], args[1]);	break;
	case Obor:	r = binop(g, Qor, args[0], args[1]);	break;
	case Oband:	r = binop(g, Qand, args[0], args[1]);	break;
	case Obxor:	r = binop(g, Qxor, args[0], args[1]);	break;
	case Obsl:	r = binop(g, Qshl, args[0], args[1]);	break;
	case Obsr:	
		if (istysigned(exprtype(n)))
			r = binop(g, Qshr, args[0], args[1]);
		else
			r = binop(g, Qshl, args[0], args[1]);
		break;
	case Obnot:	die("what's the operator for negate bits?\n");

	/* comparisons */
	case Oeq:	r = cmpop(g, op, args[0], args[1]);	break;
	case One:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ogt:	r = cmpop(g, op, args[0], args[1]);	break;
	case Oge:	r = cmpop(g, op, args[0], args[1]);	break;
	case Olt:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ole:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ofeq:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ofne:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ofgt:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ofge:	r = cmpop(g, op, args[0], args[1]);	break;
	case Oflt:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ofle:	r = cmpop(g, op, args[0], args[1]);	break;
	case Oueq:	r = cmpop(g, op, args[0], args[1]);	break;
	case Oune:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ougt:	r = cmpop(g, op, args[0], args[1]);	break;
	case Ouge:	r = cmpop(g, op, args[0], args[1]);	break;
	case Oult:	r = cmpop(g, op, args[0], args[1]);	break;
	case Oule:	r = cmpop(g, op, args[0], args[1]);	break;

	case Oasn:	r = assign(g, args[0], args[1]);	break;

	case Oslbase:	r = seqbase(g, args[0], args[1]);	break;
	case Osllen:	r = slicelen(g, rval(g, args[0]));	break;
	case Oslice:	r = genslice(g, n);			break;
	case Oidx:	r = seqbase(g, args[0], args[1]);	break;

	case Ocast:	r = simpcast(g, args[0], exprtype(n));	break;
	case Ocall:	r = gencall(g, n);			break;

	case Ovar:	r = loadvar(g, n);			break;
	case Olit:	r = loadlit(g, n);			break;
	case Osize:	r = qconst(g, size(args[0]), 'l');	break;
	case Ojmp:	o(g, Qjmp, qlabel(g, args[0]), Zq, Zq);	break;
	case Oret:
		ty = tybase(exprtype(args[0]));
		if (ty->type != Tyvoid)
			assign(g, g->retval, args[0]);
		o(g, Qjmp, g->retlbl, Zq, Zq);
		break;

	case Oderef:
		ty = tybase(exprtype(args[0]));
		if (isstacktype(ty)) {
			r = rval(g, args[0]);
		} else {
			r = temp(g, args[0]);
			o(g, loadop(ty), r, rval(g, args[0]), Zq);
		}
		break;
	case Oaddr:
		ty = tybase(exprtype(args[0]));
		if (exprop(n) == Ovar)
			r = qvar(g, n);
		else
			r = lval(g, args[0]);
		break;

	case Ocjmp:
		o(g, Qjnz, rval(g, args[0]), qlabel(g, args[1]), qlabel(g, args[2]));
		break;

	case Outag:
		l = rval(g, args[0]);
		r = qtemp(g, 'w');
		o(g, Qloadw, r, l, Zq);
		break;

	case Oucon:
		r = genucon(g, n);
		break;

	case Omemb:
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
	case Ofneg:
                die("unimplemented operator %s", opstr[exprop(n)]);
		break;

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
                die("invalid operator %s: should have been removed", opstr[exprop(n)]);
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
		o(g, Qlabel, qlabelstr(g, bb->lbls[i]), Zq, Zq);

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

void spillargs(Gen *g, Node *fn)
{	
	Loc arg, val;
	Type *ty;
	Node *a;
	size_t i;

	g->arg = zalloc(fn->func.nargs * sizeof(Loc));
	for (i = 0; i < fn->func.nargs; i++) {
		a = fn->func.args[i];
		ty = tybase(decltype(a));
		if (ty->type == Tyvoid)
			continue;
		if (isstacktype(ty)) {
			arg = qvar(g, a);
		} else {
			arg = qtemp(g, qtag(g, ty));
			val = qvar(g, a);
			o(g, storeop(ty), Zq, arg, val);
			addlocal(g, a);
		}
		g->arg[i] = arg;
		g->narg++;
	}
}

void declare(Gen *g, Node *n)
{
	size_t align, size;
	char *name;
	Type *ty;

	assert(n);
	if (n->type == Nexpr && exprop(n) == Ovar)
		n = decls[n->expr.did];
	assert(n->type == Ndecl);
	name = declname(n);
	ty = decltype(n);
	align = tyalign(ty);
	size = tysize(ty);
	fprintf(g->f, "\t%%%s =l alloc%zd %zd\n", name, align, size);
}

static const char *insnname[] = {
#define Insn(name) #name,
#include "qbe.def"
#undef Insn
};

void emitloc(Gen *g, Loc l, char *sep)
{
	char name[1024];
	Node *dcl;
	char globl;
	
	if (!l.tag)
		return;
	switch (l.kind) {
	case Ltemp:	fprintf(g->f, "%%.%lld%s", l.tmp, sep);	break;
	case Lconst:	fprintf(g->f, "%lld%s", l.cst, sep);	break;
	case Llabel:	fprintf(g->f, "@%s%s", l.lbl, sep);	break;
	case Ldecl:
		dcl = decls[l.dcl];
		globl = dcl->decl.isglobl ? '$' : '%';
		asmname(name, sizeof name, dcl->decl.name, globl);
		fprintf(g->f, "%s%s", name, sep);
	}
}

void emitinsn(Gen *g, Insn *insn)
{
	size_t i;
	char *sep;

	if (insn->op != Qlabel)
		fprintf(g->f, "\t");
	switch (insn->op) {
	case Qlabel:
		emitloc(g, insn->ret, "");
		break;
	case Qjmp:
		fprintf(g->f, "%s ", insnname[insn->op]);
		emitloc(g, insn->ret, "");
		break;
	case Qjnz:
		fprintf(g->f, "%s ", insnname[insn->op]);
		emitloc(g, insn->ret, ", ");
		emitloc(g, insn->arg[0], ", ");
		emitloc(g, insn->arg[1], "");
		break;
	case Qcall:
		emitloc(g, insn->ret, "");
		if (insn->ret.tag)
			fprintf(g->f, " =%c ", insn->ret.tag);
		fprintf(g->f, "%s ", insnname[insn->op]);
		emitloc(g, insn->fn, " ");
		fprintf(g->f, "(");
		for (i = 0; i < insn->nfarg; i++) {
			sep = (i == insn->nfarg - 1) ? "" : ", ";
			fprintf(g->f, "%s ", qtype(g, insn->fty[i]));
			emitloc(g, insn->farg[i], sep);
		}
		fprintf(g->f, ")");
		break;
	default:
		emitloc(g, insn->ret, "");
		if (insn->ret.tag)
			fprintf(g->f, " =%c ", insn->ret.tag);
		fprintf(g->f, "%s ", insnname[insn->op]);
		sep = insn->arg[1].tag ? ", " : "";
		emitloc(g, insn->arg[0], sep);
		emitloc(g, insn->arg[1], "");
		break;
	}
	fprintf(g->f, "\n");
}

void emitfn(Gen *g, Node *dcl)
{
	char name[1024], *export, *retname;
	Node *a, *n, *fn;
	Type *ty, *rtype;
	size_t i, arg;
	char *sep;

	n = dcl->decl.init;
	n = n->expr.args[0];
	fn = n->lit.fnval;
	rtype = tybase(g->rettype);
	export = isexport(dcl) ? "export" : "";
	asmname(name, sizeof name, dcl->decl.name, '$');
	retname = (rtype->type == Tyvoid) ? "" : qtype(g, rtype);
	fprintf(g->f, "%s function %s %s", export, retname, name);
	fprintf(g->f, "(");
	arg = 0;
	for (i = 0; i < fn->func.nargs; i++) {
		a = fn->func.args[i];
		ty = tybase(decltype(a));
		if (ty->type != Tyvoid) {
			sep = (arg == g->narg - 1) ? "" : ", ";
			fprintf(g->f, "%s ", qtype(g, ty));
			emitloc(g, g->arg[arg++], sep);
		}
	}
	fprintf(g->f, ")\n");
	fprintf(g->f, "{\n");
	fprintf(g->f, "@start\n");
	if (g->retval)
		declare(g, g->retval);
	for (i = 0; i < g->nlocal; i++)
		declare(g, g->local[i]);
	for (i = 0; i < g->ninsn; i++)
		emitinsn(g, &g->insn[i]);
	fprintf(g->f, "}\n");
}

void genfn(Gen *g, Node *dcl)
{
	Node *n, *b, *retdcl;
	size_t i;
	Cfg *cfg;
	Bb *bb;
	Loc r;

	if (dcl->decl.isextern || dcl->decl.isgeneric)
		return;

	n = dcl->decl.init;

	/* set up the simp context */
	/* unwrap to the function body */
	//collectenv(s, n);
	n = n->expr.args[0];
	n = n->lit.fnval;
	b = n->func.body;

	cfg = mkcfg(dcl, b->block.stmts, b->block.nstmts);
	check(cfg);
	if (debugopt['t'] || debugopt['s'])
		dumpcfg(cfg, stdout);

	/* func declaration */
	g->retval = NULL;
	g->rettype = tybase(dcl->decl.type->sub[0]);
	g->retlbl = qlabel(g, genlbl(dcl->loc));

	if (tybase(g->rettype)->type != Tyvoid)
		g->retval = gentemp(dcl->loc, g->rettype, &retdcl);
	spillargs(g, n);
	for (i = 0; i < cfg->nbb; i++) {
		bb = cfg->bb[i];
		if (!bb)
			continue;
		genbb(g, cfg, bb);
	}
	o(g, Qlabel, g->retlbl, Zq, Zq);
	if (g->retval)
		r = rval(g, g->retval);
	else
		r = Zq;
	o(g, Qret, Zq, r, Zq);
	emitfn(g, dcl);
	g->ninsn = 0;
	g->narg = 0;
	free(g->arg);
}

static void encodemin(Gen *g, uint64_t val)
{
	size_t i, shift;
	uint8_t b;

	if (val < 128) {
		fprintf(g->f, "\tb %zd,\n", val);
		return;
	}

	for (i = 1; i < 8; i++)
		if (val < 1ULL << (7*i))
			break;
	shift = 8 - i;
	b = ~0ull << (shift + 1);
	b |= val & ~(~0ull << shift);
	fprintf(g->f, "\tb %u,\n", b);
	val >>=  shift;
	while (val != 0) {
		fprintf(g->f, "\tb %u,\n", (uint)val & 0xff);
		val >>= 8;
	}
}

static void outbytes(Gen *g, char *p, size_t sz)
{
	size_t i;

	for (i = 0; i < sz; i++) {
		if (i % 60 == 0)
			fprintf(g->f, "\tb \"");
		if (p[i] == '"' || p[i] == '\\')
			fprintf(g->f, "\\");
		if (isprint(p[i]))
			fprintf(g->f, "%c", p[i]);
		else
			fprintf(g->f, "\\%03o", (uint8_t)p[i] & 0xff);
		/* line wrapping for readability */
		if (i % 60 == 59 || i == sz - 1)
			fprintf(g->f, "\",\n");
	}
}

void genblob(Gen *g, Blob *b)
{
	size_t i;

	if (b->lbl) {
		if (b->iscomdat)
			/* FIXME: emit once */
		if (b->isglobl)
			fprintf(g->f, "export ");
		fprintf(g->f, "data $%s = {\n", b->lbl);
	}

	switch (b->type) {
	case Btimin:	encodemin(g, b->ival);	break;
	case Bti8:	fprintf(g->f, "\tb %zd,\n", b->ival);	break;
	case Bti16:	fprintf(g->f, "\th %zd,\n", b->ival);	break;
	case Bti32:	fprintf(g->f, "\tw %zd,\n", b->ival);	break;
	case Bti64:	fprintf(g->f, "\tl %zd,\n", b->ival);	break;
	case Btbytes:	outbytes(g, b->bytes.buf, b->bytes.len);	break;
	case Btpad:	fprintf(g->f, "\tz %zd,\n", b->npad);	break;
	case Btref:	fprintf(g->f, "\tl $%s + %zd,\n", b->ref.str, b->ref.off);	break;
	case Btseq:
		for (i = 0; i < b->seq.nsub; i++)
			genblob(g, b->seq.sub[i]);
		break;
	}
	if(b->lbl) {
		fprintf(g->f, "}\n\n");
	}
}

void gendata(Gen *g, Node *n)
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
	fprintf(g->f, "\n");
}

void outarray(Gen *g, Type *ty)
{
	size_t sz;

	sz = 0;
	if (ty->asize)
		sz = ty->asize->expr.args[0]->lit.intval;
	outtypebody(g, ty->sub[0]);
	fprintf(g->f, "\t%s %zd,\n", qtype(g, ty->sub[0]), sz);
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
	fprintf(g->f, "%zd\n", maxsize);
}

void outtuple(Gen *g, Type *ty)
{
	size_t i;
	Type *mty;

	for (i = 0; i < ty->nsub; i++) {
		mty = ty->sub[i];
		outtypebody(g, mty);
		fprintf(g->f, "\t%s,\n", qtype(g, mty));
	}
}

void outtypebody(Gen *g, Type *ty)
{
	switch (ty->type) {
	case Tyvoid:	break;
	case Tybool:	fprintf(g->f, "\tb,\n");	break;
	case Tychar:	fprintf(g->f, "\tw,\n");	break;
	case Tyint8:	fprintf(g->f, "\tb,\n");	break;
	case Tyint16:	fprintf(g->f, "\th,\n");	break;
	case Tyint:	fprintf(g->f, "\tw,\n");	break;
	case Tyint32:	fprintf(g->f, "\tw,\n");	break;
	case Tyint64:	fprintf(g->f, "\tl,\n");	break;
	case Tybyte:	fprintf(g->f, "\tb,\n");	break;
	case Tyuint8:	fprintf(g->f, "\tb,\n");	break;
	case Tyuint16:	fprintf(g->f, "\th,\n");	break;
	case Tyuint:	fprintf(g->f, "\tw,\n");	break;
	case Tyuint32:	fprintf(g->f, "\tw,\n");	break;
	case Tyuint64:	fprintf(g->f, "\tl,\n");	break;
	case Tyflt32:	fprintf(g->f, "\ts,\n");	break;
	case Tyflt64:	fprintf(g->f, "\td,\n");	break;
	case Typtr:	fprintf(g->f, "\tl,\n");	break;
	case Tyslice:	fprintf(g->f, "\tl, l,\n");	break;
	case Tycode:	fprintf(g->f, "\tl,\n");	break;
	case Tyfunc:	fprintf(g->f, "\tl, l,\n");	break;
	case Tyvalist:	fprintf(g->f, "\tl\n");	break;
	case Tyarray:	outarray(g, ty);	break;
	case Tystruct:	outstruct(g, ty);	break;
	case Tytuple:	outtuple(g, ty);	break;
	case Tyunion:	outunion(g, ty);	break;
	case Tyname:	fprintf(g->f, "\t:t%zd,\n", ty->tid);	break;

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

void outqtype(Gen *g, Type *t)
{
	char buf[1024];
	Type *ty;
	Ty tt;

	ty = tydedup(t);
	tt = ty->type;
	if (tt == Tycode || tt == Tyvar || tt == Tyunres || hasparams(ty))
		return;
	if (ty->vis == Visbuiltin)
		return;
	if (g->typenames[ty->tid]) {
		g->typenames[t->tid] = g->typenames[ty->tid];
		return;
	}

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

	fprintf(g->f, "type %s = align %zd {\n", g->typenames[ty->tid], tyalign(ty));
	if (tt != Tyname)
		outtypebody(g, ty);
	else
		outtypebody(g, ty->sub[0]);
	fprintf(g->f, "}\n\n");
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

	/* generate the code */
	g = zalloc(sizeof(Gen));
	g->f = fopen(out, "w");
	if (!g->f)
		die("Couldn't open fd %s", out);
	/* set up code gen state */
	globls = mkht(varhash, vareq);
	initconsts(g, globls);
	fillglobls(file->file.globls, globls);

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
				g->local = locals;
				g->nlocal = nlocals;
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
	fclose(g->f);
}
