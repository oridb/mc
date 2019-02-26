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
#include "asm.h"
#include "../config.h"


/* takes a list of nodes, and reduces it (and it's subnodes) to a list
 * following these constraints:
 *      - All nodes are expression node
 *      - Nodes with side effects are root node
 *      - All nodes operate on machine-primitive types and tuple
 */
typedef
struct Simp Simp;
struct
Simp {
	int isglobl;

	Node **stmts;
	size_t nstmts;

	/* return handling */
	Node *endlbl;
	Node *ret;
	int hasenv;
	int isbigret;

	/* location handling */
	Node **blobs;
	size_t nblobs;
	Htab *globls;

	Htab *stkoff;
	Htab *envoff;
	size_t stksz;

	Node **args;
	size_t nargs;
};

static int envcmp(const void *pa, const void *pb);
static Node *simp(Simp *s, Node *n);
static Node *rval(Simp *s, Node *n, Node *dst);
static Node *lval(Simp *s, Node *n);
static Node *assign(Simp *s, Node *lhs, Node *rhs);
static Node *assignat(Simp *s, Node *r, size_t off, Node *val);
static Node *getcode(Simp *s, Node *n);
static void simpconstinit(Simp *s, Node *dcl);
static Node *simpcast(Simp *s, Node *val, Type *to);
static Node *simpslice(Simp *s, Node *n, Node *dst);
static Node *idxaddr(Simp *s, Node *seq, Node *idx);

/* useful constants */
Type *tyintptr;
Type *tyword;
Type *tyvoid;
Node *abortoob;

static void
append(Simp *s, Node *n)
{
	if (debugopt['S'])
		dumpn(n, stdout);
        assert(n->type != Ndecl);
	lappend(&s->stmts, &s->nstmts, n);
}

static int
ispure(Node *n)
{
	return opispure[exprop(n)];
}

size_t
alignto(size_t sz, Type *t)
{
	return align(sz, tyalign(t));
}

static Type *
base(Type *t)
{
	assert(t->nsub == 1);
	return t->sub[0];
}

static Node *
add(Node *a, Node *b)
{
	Node *n;

	assert(size(a) == size(b));
	n = mkexpr(a->loc, Oadd, a, b, NULL);
	n->expr.type = a->expr.type;
	return n;
}

static Node *
addk(Node *n, uvlong v)
{
	Node *k;

	k = mkintlit(n->loc, v);
	k->expr.type = exprtype(n);
	return add(n, k);
}

static Node *
sub(Node *a, Node *b)
{
	Node *n;

	n = mkexpr(a->loc, Osub, a, b, NULL);
	n->expr.type = a->expr.type;
	return n;
}

static Node *
mul(Node *a, Node *b)
{
	Node *n;

	n = mkexpr(a->loc, Omul, a, b, NULL);
	n->expr.type = a->expr.type;
	return n;
}

static int
addressable(Simp *s, Node *a)
{
	if (a->type == Ndecl || (a->type == Nexpr && exprop(a) == Ovar))
		return hthas(s->envoff, a) || hthas(s->stkoff, a) || hthas(s->globls, a);
	else
		return stacknode(a);
}

int
stacknode(Node *n)
{
	if (n->type == Nexpr)
		return isstacktype(n->expr.type);
	else
		return isstacktype(n->decl.type);
}

int
floatnode(Node *n)
{
	if (n->type == Nexpr)
		return istyfloat(n->expr.type);
	else
		return istyfloat(n->decl.type);
}

static void
forcelocal(Simp *s, Node *n)
{
	assert(n->type == Ndecl || (n->type == Nexpr && exprop(n) == Ovar));
	s->stksz += size(n);
	s->stksz = align(s->stksz, min(size(n), Ptrsz));
	if (debugopt['i']) {
		dumpn(n, stdout);
		printf("declared at %zd, size = %zd\n", s->stksz, size(n));
	}
	htput(s->stkoff, n, itop(s->stksz));
}

static void
declarelocal(Simp *s, Node *n)
{
	if (n->type == Nexpr)
		n = decls[n->decl.did];
	if (n->decl.isconst && n->decl.isextern)
		htput(s->globls, n, asmname(n));
	else if (stacknode(n))
		forcelocal(s, n);
}

/* takes the address of a node, possibly converting it to
 * a pointer to the base type 'bt' */
static Node *
addr(Simp *s, Node *a, Type *bt)
{
	Node *n;

	n = mkexpr(a->loc, Oaddr, a, NULL);
	if (!addressable(s, a))
		forcelocal(s, a);
	if (!bt)
		n->expr.type = mktyptr(a->loc, a->expr.type);
	else
		n->expr.type = mktyptr(a->loc, bt);
	return n;
}

static Node *
load(Node *a)
{
	Node *n;

	assert(a->expr.type->type == Typtr);
	n = mkexpr(a->loc, Oderef, a, NULL);
	n->expr.type = base(a->expr.type);
	return n;
}

static Node *
deref(Node *a, Type *t)
{
	Node *n;

	assert(a->expr.type->type == Typtr);
	n = mkexpr(a->loc, Oderef, a, NULL);
	if (t)
		n->expr.type = t;
	else
		n->expr.type = base(a->expr.type);
	return n;
}

static Node *
set(Node *a, Node *b)
{
	Node *n;

	assert(a != NULL && b != NULL);
	assert(exprop(a) == Ovar || exprop(a) == Oderef);
	if (tybase(exprtype(a))->type == Tyvoid)
		return a;

	n = mkexpr(a->loc, Oset, a, b, NULL);
	n->expr.type = exprtype(a);
	return n;
}

static void
def(Simp *s, Node *var)
{
	Node *d;

	d = mkexpr(var->loc, Odef, var, NULL);
	d->expr.type = mktype(var->loc, Tyvoid);
	append(s, d);
}

static Node *
disp(Srcloc loc, uint v)
{
	Node *n;

	n = mkintlit(loc, v);
	n->expr.type = tyintptr;
	return n;
}

static Node *
word(Srcloc loc, uint v)
{
	Node *n;

	n = mkintlit(loc, v);
	n->expr.type = tyword;
	return n;
}

static Node *
temp(Simp *simp, Node *e)
{
	Node *t, *dcl;

	assert(e->type == Nexpr);
	t = gentemp(e->loc, e->expr.type, &dcl);
	if (stacknode(e))
		declarelocal(simp, dcl);
	return t;
}

static Node *
slicelen(Simp *s, Node *sl)
{
	/* *(&sl + sizeof(size_t)) */
	return load(addk(addr(s, rval(s, sl, NULL), tyintptr), Ptrsz));
}

Node *
loadvar(Simp *s, Node *n, Node *dst)
{
	Node *p, *f, *r, *e;

	if (isconstfn(n)) {
		if (dst)
			r = dst;
		else
			r = temp(s, n);
		f = getcode(s, n);
		p = addr(s, r, exprtype(n));
		assignat(s, p, Ptrsz, f);
		e = mkintlit(n->loc, 0);
		e->expr.type = tyintptr;
		assignat(s, p, 0, e);
	} else {
		r = n;
	}
	return r;
}

static Node *
seqlen(Simp *s, Node *n, Type *ty)
{
	Node *t, *r;

        r = NULL;
	if (exprtype(n)->type == Tyslice) {
		t = slicelen(s, n);
		r = simpcast(s, t, ty);
	} else if (exprtype(n)->type == Tyarray) {
		t = exprtype(n)->asize;
                if (t)
                    r = simpcast(s, t, ty);
	}
	return r;
}


static Node *
uconid(Simp *s, Node *n)
{
	Ucon *uc;

	n = rval(s, n, NULL);
	if (exprop(n) != Oucon) {
		if (isenum(tybase(exprtype(n))))
			return n;
		return load(addr(s, n, mktype(n->loc, Tyuint)));
	}

	uc = finducon(exprtype(n), n->expr.args[0]);
	return word(uc->loc, uc->id);
}

static void
simpblk(Simp *s, Node *n)
{
	size_t i;

	for (i = 0; i < n->block.nstmts; i++) {
		n->block.stmts[i] = fold(n->block.stmts[i], 0);
		simp(s, n->block.stmts[i]);
	}
}

static Node *
geninitdecl(Node *init, Type *ty, Node **dcl)
{
	Node *n, *d, *r;
	char lbl[128];

	n = mkname(init->loc, genlblstr(lbl, 128, ""));
	d = mkdecl(init->loc, n, ty);
	r = mkexpr(init->loc, Ovar, n, NULL);

	d->decl.init = init;
	d->decl.type = ty;
	d->decl.isconst = 1;
	d->decl.isglobl = 1;

	r->expr.did = d->decl.did;
	r->expr.type = ty;
	r->expr.isconst = 1;
	if (dcl)
		*dcl = d;
	return r;
}

static Node *
simpcode(Simp *s, Node *fn)
{
	Node *r, *d;

	r = geninitdecl(fn, codetype(exprtype(fn)), &d);
	r->decl.isconst = 1;
	r->decl.isglobl = 1;
	htput(s->globls, d, asmname(d));
	lappend(&file.stmts, &file.nstmts, d);
	return r;
}

static Node *
simpblob(Simp *s, Node *blob)
{
	Node *r, *d;

	r = geninitdecl(blob, exprtype(blob), &d);
	htput(s->globls, d, asmname(d));
	lappend(&s->blobs, &s->nblobs, d);
	return r;
}

static Node *
ptrsized(Simp *s, Node *v)
{
	if (size(v) == Ptrsz)
		return v;
	else if (size(v) < Ptrsz)
		v = mkexpr(v->loc, Ozwiden, v, NULL);
	else if (size(v) > Ptrsz)
		v = mkexpr(v->loc, Otrunc, v, NULL);
	v->expr.type = tyintptr;
	return v;
}

static Node *
membaddr(Simp *s, Node *n)
{
	Node *t, *u, *r;
	Node **args;
	Type *ty;

	args = n->expr.args;
	ty = tybase(exprtype(args[0]));
	if (ty->type == Typtr) {
		t = lval(s, args[0]);
	} else {
		t = addr(s, lval(s, args[0]), exprtype(n));
	}
	u = disp(n->loc, offset(args[0], args[1]));
	r = add(t, u);
	r->expr.type = mktyptr(n->loc, n->expr.type);
	return r;
}

static void
checkidx(Simp *s, Op op, Node *len, Node *idx)
{
	Node *cmp, *die, *jmp;
	Node *ok, *fail;

	if (!len)
		return;
	/* create expressions */
	cmp = mkexpr(idx->loc, op, ptrsized(s, idx), ptrsized(s, len), NULL);
	cmp->expr.type = mktype(len->loc, Tybool);
	ok = genlbl(len->loc);
	fail = genlbl(len->loc);
	die = mkexpr(idx->loc, Ocall, abortoob, NULL);
	die->expr.type = mktype(len->loc, Tyvoid);

	/* insert them */
	jmp = mkexpr(idx->loc, Ocjmp, cmp, ok, fail, NULL);
	append(s, jmp);
	append(s, fail);
	append(s, die);
	append(s, ok);
}

static Node *
idxaddr(Simp *s, Node *seq, Node *idx)
{
	Node *a, *t, *u, *v, *w; /* temps */
	Node *r; /* result */
	Type *ty, *seqty;
	size_t sz;

	a = rval(s, seq, NULL);
	seqty = tybase(exprtype(seq));
	ty = seqty->sub[0];
	if (seqty->type == Tyarray) {
		t = addr(s, a, ty);
		w = seqty->asize;
	} else if (seqty->type == Tyslice) {
		t = load(addr(s, a, mktyptr(seq->loc, ty)));
		w = slicelen(s, a);
	} else {
		die("can't index type %s", tystr(seq->expr.type));
	}
	assert(exprtype(t)->type == Typtr);
	u = rval(s, idx, NULL);
	u = ptrsized(s, u);
	checkidx(s, Olt, w, u);
	sz = tysize(ty);
	v = mul(u, disp(seq->loc, sz));
	r = add(t, v);
	return r;
}

static Node *
slicebase(Simp *s, Node *n, Node *off)
{
	Node *u, *v;
	Type *ty;
	int sz;

	u = NULL;
	ty = tybase(exprtype(n));
	switch (ty->type) {
	case Typtr:	u = n;	break;
	case Tyslice:	u = load(addr(s, n, mktyptr(n->loc, base(exprtype(n)))));	break;
	case Tyarray:	u = addr(s, n, base(exprtype(n)));	break;
	default: die("Unslicable type %s", tystr(n->expr.type));
	}
	/* safe: all types we allow here have a sub[0] that we want to grab */
	if (off) {
		off = ptrsized(s, rval(s, off, NULL));
		sz = tysize(n->expr.type->sub[0]);
		v = mul(off, disp(n->loc, sz));
		return add(u, v);
	} else {
		return u;
	}
}

static Node *
loadidx(Simp *s, Node *arr, Node *idx)
{
	Node *v, *a;

	a = rval(s, arr, NULL);
	v = deref(idxaddr(s, a, idx), NULL);
	return v;
}

static Node *
lval(Simp *s, Node *n)
{
	Node *r;
	Node **args;

	args = n->expr.args;
	switch (exprop(n)) {
	case Ovar:	r = loadvar(s, n, NULL);	break;
	case Oidx:	r = loadidx(s, args[0], args[1]);	break;
	case Oderef:	r = deref(rval(s, args[0], NULL), NULL);	break;
	case Otupmemb:	r = rval(s, n, NULL);	break;
	case Omemb:	r = rval(s, n, NULL);	break;
	case Ostruct:	r = rval(s, n, NULL);	break;
	case Oucon:	r = rval(s, n, NULL);	break;
	case Oarr:	r = rval(s, n, NULL);	break;

	/*
	  not expressible as lvalues in syntax, but
	  we take their address in matches
	 */
	case Oudata:    r = rval(s, n, NULL); break;
	case Outag:     r = rval(s, n, NULL); break;
	case Otupget:   r = rval(s, n, NULL); break;

	default:
			fatal(n, "%s cannot be an lvalue", opstr[exprop(n)]);
			break;
	}
	return r;
}

static Node *
intconvert(Simp *s, Node *from, Type *to, int issigned)
{
	Node *r;
	size_t fromsz, tosz;

	fromsz = size(from);
	tosz = tysize(to);
	r = rval(s, from, NULL);
	if (fromsz > tosz) {
		r = mkexpr(from->loc, Otrunc, r, NULL);
	} else if (tosz > fromsz) {
		if (issigned)
			r = mkexpr(from->loc, Oswiden, r, NULL);
		else
			r = mkexpr(from->loc, Ozwiden, r, NULL);
	}
	r->expr.type = to;
	return r;
}

static Node *
simpcast(Simp *s, Node *val, Type *to)
{
	Node *r;
	Type *t;

	r = NULL;
	t = tybase(exprtype(val));
	if (tyeq(tybase(to), tybase(t))) {
		r = rval(s, val, NULL);
		r->expr.type = to;
		return r;
	}
	/* do the type conversion */
	switch (tybase(to)->type) {
	case Tybool:
	case Tyint8: case Tyint16: case Tyint32: case Tyint64:
	case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
	case Tyint: case Tyuint: case Tychar: case Tybyte:
	case Typtr:
		switch (t->type) {
			/* ptr -> slice conversion is disallowed */
		case Tyslice:
			/* FIXME: we should only allow casting to pointers. */
			if (tysize(to) != Ptrsz)
				fatal(val, "bad cast from %s to %s", tystr(exprtype(val)), tystr(to));
			val = rval(s, val, NULL);
			r = slicebase(s, val, NULL);
			break;
		case Tyfunc:
			if (to->type != Typtr)
				fatal(val, "bad cast from %s to %s", tystr(exprtype(val)), tystr(to));
			r = getcode(s, val);
			break;
			/* signed conversions */
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyint:
			r = intconvert(s, val, to, 1);
			break;
			/* unsigned conversions */
		case Tybool:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyuint: case Tychar: case Tybyte:
		case Typtr:
			r = intconvert(s, val, to, 0);
			break;
		case Tyflt32: case Tyflt64:
			if (tybase(to)->type == Typtr)
				fatal(val, "bad cast from %s to %s",
						tystr(exprtype(val)), tystr(to));
			r = mkexpr(val->loc, Oflt2int, rval(s, val, NULL), NULL);
			r->expr.type = to;
			break;
		default:
			fatal(val, "bad cast from %s to %s",
					tystr(exprtype(val)), tystr(to));
		}
		break;
	case Tyflt32: case Tyflt64:
		switch (t->type) {
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyint: case Tyuint: case Tychar: case Tybyte:
			r = mkexpr(val->loc, Oint2flt, rval(s, val, NULL), NULL);
			r->expr.type = to;
			break;
		case Tyflt32: case Tyflt64:
			r = mkexpr(val->loc, Oflt2flt, rval(s, val, NULL), NULL);
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

/* Simplifies taking a slice of an array, pointer,
 * or other slice down to primitive pointer operations */
static Node *
simpslice(Simp *s, Node *n, Node *dst)
{
	Node *t;
	Node *start, *end, *arg;
	Node *seq, *base, *sz, *len, *max;
	Node *stbase, *stlen;

	if (dst)
		t = dst;
	else
		t = temp(s, n);
	arg = n->expr.args[0];
	if (exprop(arg) == Oarr && arg->expr.nargs == 0) {
		seq = arg;
		base = mkintlit(n->loc, 0);
		base->expr.type = tyintptr;
	} else {
		/* *(&slice) = (void*)base + off*sz */
		seq = rval(s, arg, NULL);
		base = slicebase(s, seq, n->expr.args[1]);
	}
	start = ptrsized(s, rval(s, n->expr.args[1], NULL));
	end = ptrsized(s, rval(s, n->expr.args[2], NULL));
	len = sub(end, start);
	/* we can be storing through a pointer, in the case
	 * of '*foo = bar'. */
	max = seqlen(s, seq, tyword);
	if (max)
		checkidx(s, Ole, max, end);
	if (tybase(exprtype(t))->type == Typtr) {
		stbase = set(simpcast(s, t, mktyptr(t->loc, tyintptr)), base);
		sz = addk(simpcast(s, t, mktyptr(t->loc, tyintptr)), Ptrsz);
	} else {
		stbase = set(deref(addr(s, t, tyintptr), NULL), base);
		sz = addk(addr(s, t, tyintptr), Ptrsz);
	}
	/* *(&slice + ptrsz) = len */
	stlen = set(deref(sz, NULL), len);
	append(s, stbase);
	append(s, stlen);
	return t;
}

static Node *
visit(Simp *s, Node *n)
{
	size_t i;
	Node *r;

	for (i = 0; i < n->expr.nargs; i++)
		n->expr.args[i] = rval(s, n->expr.args[i], NULL);
	if (ispure(n)) {
		r = n;
	} else {
		if (exprtype(n)->type == Tyvoid) {
			r = NULL;
			append(s, n);
		} else {
			r = temp(s, n);
			append(s, set(r, n));
		}
	}
	return r;
}

static Node *
tupget(Simp *s, Node *tup, size_t idx, Node *dst)
{
	Node *plv, *prv, *sz, *stor, *dcl;
	size_t off, i;
	Type *ty;

	off = 0;
	ty = exprtype(tup);
	for (i = 0; i < ty->nsub; i++) {
		off = alignto(off, ty->sub[i]);
		if (i == idx)
			break;
		off += tysize(ty->sub[i]);
	}

	if (!dst) {
		dst = gentemp(tup->loc, ty->sub[idx], &dcl);
		if (isstacktype(ty->sub[idx]))
			declarelocal(s, dcl);
	}
	prv = add(addr(s, tup, ty->sub[i]), disp(tup->loc, off));
	if (stacknode(dst)) {
		sz = disp(dst->loc, size(dst));
		plv = addr(s, dst, exprtype(dst));
		stor = mkexpr(dst->loc, Oblit, plv, prv, sz, NULL);
	} else {
		stor = set(dst, load(prv));
	}
	append(s, stor);
	return dst;
}

static Node *
assign(Simp *s, Node *lhs, Node *rhs)
{
	Node *t, *u, *v, *r;

	t = lval(s, lhs);
	u = rval(s, rhs, t);

	if (tybase(exprtype(lhs))->type == Tyvoid)
		return u;

	/* hack: we're assigning to lhs, but blitting shit over doesn't
	 * trigger that */
	if (stacknode(lhs))
		def(s, lhs);
	/* if we stored the result into t, rval() should return that,
	 * so we know our work is done. */
	if (u == t) {
		r = t;
	} else if (stacknode(lhs)) {
		t = addr(s, t, exprtype(lhs));
		u = addr(s, u, exprtype(lhs));
		v = disp(lhs->loc, size(lhs));
		r = mkexpr(lhs->loc, Oblit, t, u, v, NULL);
		r->expr.type = exprtype(lhs);
	} else {
		r = set(t, u);
	}
	return r;
}

static Node *
assignat(Simp *s, Node *r, size_t off, Node *val)
{
	Node *pval, *pdst;
	Node *sz;
	Node *st;

	pdst = add(r, disp(val->loc, off));

	if (stacknode(val)) {
		sz = disp(val->loc, size(val));
		pval = addr(s, val, exprtype(val));
		st = mkexpr(val->loc, Oblit, pdst, pval, sz, NULL);
	} else {
		st = set(deref(pdst, val->expr.type), val);
	}
	append(s, st);
	return r;
}

/* Simplify tuple construction to a stack allocated
 * value by evaluating the rvalue of each node on the
 * rhs and assigning it to the correct offset from the
 * head of the tuple. */
static Node *
simptup(Simp *s, Node *n, Node *dst)
{
	Node **args;
	Node *r;
	size_t i, off;

	args = n->expr.args;
	if (!dst)
		dst = temp(s, n);
	r = addr(s, dst, exprtype(dst));

	off = 0;
	for (i = 0; i < n->expr.nargs; i++) {
		off = alignto(off, exprtype(args[i]));
		assignat(s, r, off, rval(s, args[i], NULL));
		off += size(args[i]);
	}
	return dst;
}

static Node *
simpucon(Simp *s, Node *n, Node *dst)
{
	Node *tmp, *u, *tag, *elt, *sz;
	Node *r;
	Type *ty;
	Ucon *uc;
	size_t o;

	/* find the ucon we're constructing here */
	ty = tybase(exprtype(n));
	uc = finducon(ty, n->expr.args[0]);
	if (!uc)
		die("Couldn't find union constructor");

	if (dst)
		tmp = dst;
	else
		tmp = temp(s, n);

	if (isenum(ty)) {
		/* enums are treated as integers
		 * by the backend */
		append(s, set(tmp, n));
		return tmp;
	}

	/* Set the tag on the ucon */
	u = addr(s, tmp, mktype(n->loc, Tyuint));
	tag = mkintlit(n->loc, uc->id);
	tag->expr.type = mktype(n->loc, Tyuint);
	append(s, set(deref(u, tyword), tag));


	/* fill the value, if needed */
	if (!uc->etype)
		return tmp;
	elt = rval(s, n->expr.args[1], NULL);
	o = alignto(Wordsz, uc->etype);
	u = addk(u, o);
	if (isstacktype(uc->etype)) {
		elt = addr(s, elt, uc->etype);
		sz = disp(n->loc, tysize(uc->etype));
		r = mkexpr(n->loc, Oblit, u, elt, sz, NULL);
	} else {
		r = set(deref(u, uc->etype), elt);
	}
	append(s, r);
	return tmp;
}

static Node *
simpuget(Simp *s, Node *n, Node *dst)
{
	Node *u, *p, *l;
	size_t o;

	if (!dst)
		dst = temp(s, n);
	u = rval(s, n->expr.args[0], NULL);
	o = alignto(Wordsz, exprtype(n));
	p = addk(addr(s, u, exprtype(n)), o);
	l = assign(s, dst, load(p));
	append(s, l);
	return dst;
}



static Node *
vatypeinfo(Simp *s, Node *n)
{
	Node *ti, *tp, *td, *tn;
	Type *ft, *vt, **st;
	size_t nst, i;
	char buf[1024];

	st = NULL;
	nst = 0;
	ft = exprtype(n->expr.args[0]);
	/* The structure of ft->sub:
	 *   [return, normal, args, ...]
	 *
	 * The structure of n->expr.sub:
	 *    [fn, normal, args, , variadic, args]
	 *
	 * We want to start at variadic, so we want
	 * to count from ft->nsub - 1, up to n->expr.nsub.
	 */
	for (i = ft->nsub - 1; i < n->expr.nargs; i++) {
		lappend(&st, &nst, exprtype(n->expr.args[i]));
	}
	vt = mktytuple(n->loc, st, nst);
	tagreflect(vt);

	/* make the decl */
	tn = mkname(Zloc, tydescid(buf, sizeof buf, vt));
	td = mkdecl(Zloc, tn, mktype(n->loc, Tybyte));
	td->decl.isglobl = 1;
	td->decl.isconst = 1;
	td->decl.ishidden = 1;

	/* and the var */
	ti = mkexpr(Zloc, Ovar, tn, NULL);
	ti->expr.type = td->decl.type;
	ti->expr.did = td->decl.did;

	/* and the pointer */
	tp = mkexpr(Zloc, Oaddr, ti, NULL);
	tp->expr.type = mktyptr(n->loc, td->decl.type);

	htput(s->globls, td, asmname(td));
	return tp;
}

static Node *
capture(Simp *s, Node *n, Node *dst)
{
	Node *fn, *t, *f, *e, *val, *dcl, *fp, *envsz;
	size_t nenv, nenvt, off, sz, i;
	Type **envt;
	Node **env;

	f = simpcode(s, n);
	fn = n->expr.args[0];
	fn = fn->lit.fnval;
	if (!dst) {
		dst = gentemp(n->loc, closuretype(exprtype(f)), &dcl);
		forcelocal(s, dcl);
	}
	fp = addr(s, dst, exprtype(dst));
	assignat(s, fp, Ptrsz, f);

	env = getclosure(fn->func.scope, &nenv);
	if (env) {
		/*
		 * we need these in a deterministic order so that we can
		 * put them in the right place both when we use them and
		 * when we capture them. 
		 */
		qsort(env, nenv, sizeof(Node*), envcmp);

		/* make the tuple that will hold the environment */
		envt = NULL;
		nenvt = 0;
		/* reserve space for size */
		lappend(&envt, &nenvt, tyintptr);
		sz = Ptrsz;
		for (i = 0; i < nenv; i++) {
			lappend(&envt, &nenvt, decltype(env[i]));
			sz += size(env[i]);
			sz = alignto(sz, decltype(env[i]));
		}

		t = gentemp(n->loc, mktytuple(n->loc, envt, nenvt), &dcl);
		forcelocal(s, dcl);
		e = addr(s, t, exprtype(t));
		envsz = mkintlit(n->loc, sz);
		envsz->expr.type = tyintptr;
		assignat(s, e, 0, envsz);
		assignat(s, fp, 0, e);

		off = Ptrsz;    /* we start with the size of the env */
		for (i = 0; i < nenv; i++) {
			off = alignto(off, decltype(env[i]));
			val = mkexpr(n->loc, Ovar, env[i]->decl.name, NULL);
			val->expr.type = env[i]->decl.type;
			val->expr.did = env[i]->decl.did;
			assignat(s, e, off, rval(s, val, NULL));
			off += size(env[i]);
		}
		free(env);
	} else {
		/*
		 * We need to zero out the environment, so that
		 * duplicating the function doesn't think we have
		 * a bogus environment.
		 */
		e = mkintlit(n->loc, 0);
		e->expr.type = tyintptr;
		assignat(s, fp, 0, e);
	}
	return dst;
}

static Node *
getenvptr(Simp *s, Node *n)
{
	assert(tybase(exprtype(n))->type == Tyfunc);
	return load(addr(s, n, tyintptr));
}

static Node *
getcode(Simp *s, Node *n)
{
	Node *r, *p, *d;
	Type *ty;

	if (isconstfn(n)) {
		d = decls[n->expr.did];
		r = mkexpr(n->loc, Ovar, n->expr.args[0], NULL);
		r->expr.did = d->decl.did;
		r->expr.type = codetype(exprtype(n));
	} else {
		ty = tybase(exprtype(n));
		assert(ty->type == Tyfunc);
		p = addr(s, rval(s, n, NULL), codetype(ty));
		r = load(addk(p, Ptrsz));
	}
	return r;
}

static Node *
simpcall(Simp *s, Node *n, Node *dst)
{
	Node *r, *call, *fn;
	size_t i, nargs;
	Node **args;
	Type *ft;
	Op op;

	/* NB: If we called rval() on a const function, we would end up with
	   a stack allocated closure. We don't want to do this. */
	fn = n->expr.args[0];
	if (!isconstfn(fn))
		fn = rval(s, fn, NULL);
	ft = tybase(exprtype(fn));
	if (exprtype(n)->type == Tyvoid)
		r = NULL;
	else if (isstacktype(exprtype(n)) && dst)
		r = dst;
	else
		r = temp(s, n);

	args = NULL;
	nargs = 0;
	op = Ocall;
	lappend(&args, &nargs, getcode(s, fn));
	if (!isconstfn(fn)) {
		op = Ocallind;
		lappend(&args, &nargs, getenvptr(s, fn));
	}

	if (exprtype(n)->type != Tyvoid && isstacktype(exprtype(n)))
		lappend(&args, &nargs, addr(s, r, exprtype(n)));

	for (i = 1; i < n->expr.nargs; i++) {
		if (i < ft->nsub && tybase(ft->sub[i])->type == Tyvalist)
			lappend(&args, &nargs, vatypeinfo(s, n));
		if (tybase(exprtype(n->expr.args[i]))->type == Tyvoid)
			continue;
		lappend(&args, &nargs, rval(s, n->expr.args[i], NULL));
		if (exprop(n->expr.args[i]) == Oaddr)
			if (exprop(n->expr.args[i]->expr.args[0]) == Ovar)
				def(s, n->expr.args[i]->expr.args[0]);
	}
	if (i < ft->nsub && tybase(ft->sub[i])->type == Tyvalist)
		lappend(&args, &nargs, vatypeinfo(s, n));

	if (r)
		def(s, r);

	call = mkexprl(n->loc, op, args, nargs);
	call->expr.type = exprtype(n);
	if (r && !isstacktype(exprtype(n))) {
		append(s, set(r, call));
	} else {
		append(s, call);
	}
	return r;
}

static Node *
rval(Simp *s, Node *n, Node *dst)
{
	Node *t, *u, *v; /* temporary nodes */
	Node *r; /* expression result */
	Node **args;
	vlong idx, nelt;
	size_t i;
	Type *ty;

	r = NULL;
	args = n->expr.args;
	switch (exprop(n)) {
	case Osize:
		r = mkintlit(n->loc, size(args[0]));
		r->expr.type = exprtype(n);
		break;
	case Oslice:
		r = simpslice(s, n, dst);
		break;
	case Oidx:
		t = rval(s, n->expr.args[0], NULL);
		u = idxaddr(s, t, n->expr.args[1]);
		r = load(u);
		break;
	case Otupmemb:
	case Omemb:
		t = membaddr(s, n);
		r = load(t);
		break;
	case Osllen:
		r = seqlen(s, args[0], exprtype(n));
		break;
	case Oucon:
		r = simpucon(s, n, dst);
		break;
	case Outag:
		r = uconid(s, args[0]);
		break;
	case Oudata:
		r = simpuget(s, n, dst);
		break;
	case Otup:
		r = simptup(s, n, dst);
		break;
	case Oarr:
		if (!dst)
			dst = temp(s, n);
		ty = exprtype(dst);
		t = addr(s, dst, exprtype(dst));
		if (!getintlit(ty->asize, &nelt))
			die("array missing size");
		/* we only need to clear if we don't have things fully initialized */
		idx = 0;
		if (nelt != n->expr.nargs)
			append(s, mkexpr(n->loc, Oclear, t, mkintlit(n->loc, size(n)), NULL));
		for (i = 0; i < n->expr.nargs; i++) {
			if (!args[i]->expr.idx)
				idx++;
			else if (!getintlit(args[i]->expr.idx, &idx))
				die("non-numeric array size");
			assert(idx < nelt);
			assignat(s, t, size(args[i])*idx, rval(s, args[i], NULL));
		}
		r = dst;
		break;
	case Ostruct:
		if (!dst)
		dst = temp(s, n);
		u = mkexpr(dst->loc, Odef, dst, NULL);
		u->expr.type = mktype(u->loc, Tyvoid);
		append(s, u);
		t = addr(s, dst, exprtype(dst));
		ty = exprtype(n);
		/* we only need to clear if we don't have things fully initialized */
		if (tybase(ty)->nmemb != n->expr.nargs)
			append(s, mkexpr(n->loc, Oclear, t, mkintlit(n->loc, size(n)), NULL));
		for (i = 0; i < n->expr.nargs; i++)
			assignat(s, t, offset(n, args[i]->expr.idx), rval(s, args[i], NULL));
		r = dst;
		break;
	case Ocast:
		r = simpcast(s, args[0], exprtype(n));
		break;

	/* ++expr(x)
	 *  => args[0] = args[0] + 1
	 *     expr(x) */
	case Olit:
		switch (args[0]->lit.littype) {
		case Lvoid:
		case Lchr:
		case Lbool:
		case Llbl:
			r = n;
			break;
		case Lint:
			/* we can only have up to 4 byte immediates, but they
			 * can be moved into 64 bit regs */
			if ((uint64_t)args[0]->lit.intval < 0x7fffffffULL)
				r = n;
			else
				r = simpblob(s, n);
			break;
		case Lstr: case Lflt:
			r = simpblob(s, n);
			break;
		case Lfunc:
			r = capture(s, n, dst);
			break;
		}
		break;
	case Ovar:
		r = loadvar(s, n, dst);
		break;;
	case Ogap:
		fatal(n, "'_' may not be an rvalue");
		break;
	case Oret:
		if (s->isbigret) {
			t = rval(s, args[0], NULL);
			t = addr(s, t, exprtype(args[0]));
			u = disp(n->loc, size(args[0]));
			v = mkexpr(n->loc, Oblit, s->ret, t, u, NULL);
			append(s, v);
		} else {
			t = s->ret;
			u = rval(s, args[0], NULL);
			/* void calls return nothing */
			if (t) {
				t = set(t, u);
				append(s, t);
			}
		}
		append(s, mkexpr(n->loc, Oret, NULL));
		break;
	case Oasn:
		r = assign(s, args[0], args[1]);
		break;
	case Ocall:
		r = simpcall(s, n, dst);
		break;
	case Oaddr:
		t = lval(s, args[0]);
		if (exprop(t) == Ovar) /* Ovar is the only one that doesn't return Oderef(Oaddr(...)) */
			r = addr(s, t, exprtype(t));
		else
			r = t->expr.args[0];
		break;
	case Oneg:
		if (istyfloat(exprtype(n))) {
			t = mkfloat(n->loc, -1.0);
			u = mkexpr(n->loc, Olit, t, NULL);
			t->lit.type = n->expr.type;
			u->expr.type = n->expr.type;
			v = simpblob(s, u);
			r = mkexpr(n->loc, Ofmul, v, rval(s, args[0], NULL), NULL);
			r->expr.type = n->expr.type;
		} else {
			r = visit(s, n);
		}
		break;
	case Otupget:
		assert(exprop(args[1]) == Olit);
		i = args[1]->expr.args[0]->lit.intval;
		t = rval(s, args[0], NULL);
		r = tupget(s, t, i, dst);
		break;
	case Olor: case Oland:
	case Oaddeq: case Osubeq: case Omuleq: case Odiveq: case Omodeq:
	case Oboreq: case Obandeq: case Obxoreq: case Obsleq: case Obsreq:
	case Opreinc: case Opredec:
	case Opostinc: case Opostdec:
                die("invalid operator: should have been removed");
	case Obad:
		die("bad operator");
		break;
	default:
		if (istyfloat(exprtype(n))) {
			switch (exprop(n)) {
			case Oadd:	n->expr.op = Ofadd;	break;
			case Osub:	n->expr.op = Ofsub;	break;
			case Omul:	n->expr.op = Ofmul;	break;
			case Odiv:	n->expr.op = Ofdiv;	break;
			default:	break;
			}
		}
		r = visit(s, n);
		break;
	}
	return r;
}

static void
declarearg(Simp *s, Node *n)
{
	assert(n->type == Ndecl || (n->type == Nexpr && exprop(n) == Ovar));
	lappend(&s->args, &s->nargs, n);
}

static int
islbl(Node *n)
{
	Node *l;
	if (exprop(n) != Olit)
		return 0;
	l = n->expr.args[0];
	return l->type == Nlit && l->lit.littype == Llbl;
}

static Node *
simp(Simp *s, Node *n)
{
	Node *r;

	if (!n)
		return NULL;
	r = NULL;
	switch (n->type) {
	case Nblock:	simpblk(s, n);	break;
	case Ndecl:     declarelocal(s, n);     break;
	case Nexpr:
		if (islbl(n))
			append(s, n);
		else
			r = rval(s, n, NULL);
		if (r)
			append(s, r);
		break;
	default:
		dumpn(n, stderr);
		die("bad node passsed to simp()");
		break;
	}
	return r;
}

/*
 * Turns a deeply nested function body into a flatter
 * and simpler representation, which maps easily and
 * directly to assembly instructions.
 */
static void
simpinit(Simp *s, Node *f)
{
	Node *dcl;
	Type *ty;
	size_t i;

	assert(f->type == Nfunc);
	s->nstmts = 0;
	s->stmts = NULL;
	s->endlbl = genlbl(f->loc);
	s->ret = NULL;

	/* make a temp for the return type */
	ty = f->func.type->sub[0];
	if (isstacktype(ty)) {
		s->isbigret = 1;
		s->ret = gentemp(f->loc, mktyptr(f->loc, ty), &dcl);
		declarearg(s, dcl);
	} else if (tybase(ty)->type != Tyvoid) {
		s->isbigret = 0;
		s->ret = gentemp(f->loc, ty, &dcl);
	}

	for (i = 0; i < f->func.nargs; i++) {
		declarearg(s, f->func.args[i]);
	}
	simp(s, f->func.body);

	append(s, s->endlbl);
}

static int
isexport(Node *dcl)
{
	Node *n;

	/* Vishidden should also be exported. */
	if (dcl->decl.vis != Visintern)
		return 1;
	n = dcl->decl.name;
	if (!n->name.ns && streq(n->name.name, "main"))
		return 1;
	if (streq(n->name.name, "__init__") || streq(n->name.name, "__fini__"))
		return 1;
	return 0;
}

static int
envcmp(const void *pa, const void *pb)
{
	const Node *a, *b;

	a = *(const Node**)pa;
	b = *(const Node**)pb;
	return b->decl.did - a->decl.did;
}

static void
collectenv(Simp *s, Node *fn)
{
	size_t nenv, i;
	Node **env;
	size_t off;

	env = getclosure(fn->func.scope, &nenv);
	if (!env)
		return;
	/*
           we need these in a deterministic order so that we can
           put them in the right place both when we use them and
           when we capture them.
         */
	s->hasenv = 1;
	qsort(env, nenv, sizeof(Node*), envcmp);
	off = Ptrsz;    /* we start with the size of the env */
	for (i = 0; i < nenv; i++) {
		off = alignto(off, decltype(env[i]));
		htput(s->envoff, env[i], itop(off));
		off += size(env[i]);
	}
	free(env);
}

static Func *
simpfn(Simp *s, char *name, Node *dcl)
{
	Node *n;
	size_t i;
	Func *fn;
	Cfg *cfg;

	n = dcl->decl.init;
	if(debugopt['i'] || debugopt['F'] || debugopt['f'])
		printf("\n\nfunction %s\n", name);

	/* set up the simp context */
	/* unwrap to the function body */
	n = n->expr.args[0];
	n = n->lit.fnval;
	collectenv(s, n);
	simpinit(s, n);

	if (debugopt['f'] || debugopt['F'])
		for (i = 0; i < s->nstmts; i++)
			dumpn(s->stmts[i], stdout);
	for (i = 0; i < s->nstmts; i++) {
		if (s->stmts[i]->type != Nexpr)
			continue;
		s->stmts[i] = fold(s->stmts[i], 0);
	}

	cfg = mkcfg(dcl, s->stmts, s->nstmts);
	check(cfg);
	if (debugopt['t'] || debugopt['s'])
		dumpcfg(cfg, stdout);

	fn = zalloc(sizeof(Func));
	fn->name = strdup(name);
	fn->loc = dcl->loc;
	fn->type = dcl->decl.type;
	fn->isexport = isexport(dcl);
	fn->stksz = align(s->stksz, 8);
	fn->stkoff = s->stkoff;
	fn->envoff = s->envoff;
	fn->ret = s->ret;
	fn->args = s->args;
	fn->nargs = s->nargs;
	fn->cfg = cfg;
	fn->hasenv = s->hasenv;
	return fn;
}

static void
extractsub(Simp *s, Node *e)
{
	size_t i;
	Node *sub;

	assert(e != NULL);
	switch (exprop(e)) {
	case Oslice:
		sub = e->expr.args[0];
		extractsub(s, sub);
		if (exprop(sub) == Oarr) {
			if (sub->expr.nargs > 0) {
				e->expr.args[0] = simpblob(s, e->expr.args[0]);
			} else  {
				e->expr.args[0] = mkintlit(e->loc, 0);
				e->expr.args[0]->expr.type = tyintptr;
			}
		}
		break;
	case Oarr:
	case Ostruct:
		for (i = 0; i < e->expr.nargs; i++)
			extractsub(s, e->expr.args[i]);
		break;
	case Oucon:
		if (e->expr.nargs == 2)
			extractsub(s, e->expr.args[1]);
		break;
	default:
		break;
	}
}

static void
simpconstinit(Simp *s, Node *dcl)
{
	Node *e;

	dcl->decl.init = fold(dcl->decl.init, 1);;
	e = dcl->decl.init;
	if (e && exprop(e) == Olit) {
		if (e->expr.args[0]->lit.littype == Lfunc)
			dcl->decl.init = simpcode(s, e);
		lappend(&s->blobs, &s->nblobs, dcl);
	} else if (!dcl->decl.isconst && !e) {
		lappend(&s->blobs, &s->nblobs, dcl);
	} else if (e->expr.isconst) {
		switch (exprop(e)) {
		case Oarr:
		case Ostruct:
		case Oslice:
		case Oucon:
		case Ovar:
			extractsub(s, e);
			lappend(&s->blobs, &s->nblobs, dcl);
			break;
		default:
			fatal(dcl, "unsupported initializer for %s", declname(dcl));
			break;
		}
	} else {
		die("Non-constant initializer for %s\n", declname(dcl));
	}
}

void
simpglobl(Node *dcl, Htab *globls, Func ***fn, size_t *nfn, Node ***blob, size_t *nblob)
{
	Simp s = {0,};
	char *name;
	Func *f;

	s.stkoff = mkht(varhash, vareq);
	s.envoff = mkht(varhash, vareq);
	s.globls = globls;
	s.blobs = *blob;
	s.nblobs = *nblob;
	s.hasenv = 0;

	if (dcl->decl.isextern || dcl->decl.isgeneric)
		return;

	name = asmname(dcl);
	if (isconstfn(dcl)) {
		f = simpfn(&s, name, dcl);
		lappend(fn, nfn, f);
	} else {
		simpconstinit(&s, dcl);
	}
	*blob = s.blobs;
	*nblob = s.nblobs;

	free(name);
}
