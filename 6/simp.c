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
#include "mi.h"
#include "asm.h"
#include "../config.h"


/* takes a list of nodes, and reduces it (and it's subnodes) to a list
 * following these constraints:
 *      - All nodes are expression nodes
 *      - Nodes with side effects are root nodes
 *      - All nodes operate on machine-primitive types and tuples
 */
typedef struct Simp Simp;
struct Simp {
	int isglobl;

	Node **stmts;
	size_t nstmts;

	/* return handling */
	Node *endlbl;
	Node *ret;
	int hasenv;
	int isbigret;

	/* the array we're indexing for context within [] */
	Node **idxctx;
	size_t nidxctx;

	/* pre/postinc handling */
	Node **incqueue;
	size_t nqueue;

	/* break/continue handling */
	Node **loopstep;
	size_t nloopstep;
	Node **loopexit;
	size_t nloopexit;

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
static void simpcond(Simp *s, Node *n, Node *ltrue, Node *lfalse);
static void simpconstinit(Simp *s, Node *dcl);
static Node *simpcast(Simp *s, Node *val, Type *to);
static Node *simpslice(Simp *s, Node *n, Node *dst);
static Node *idxaddr(Simp *s, Node *seq, Node *idx);

/* useful constants */
Type *tyintptr;
Type *tyword;
Type *tyvoid;
Node *abortoob;

static void append(Simp *s, Node *n)
{
	if (debugopt['S'])
		dump(n, stdout);
	lappend(&s->stmts, &s->nstmts, n);
}

static int ispure(Node *n)
{
	return opispure[exprop(n)];
}

size_t alignto(size_t sz, Type *t)
{
	return align(sz, tyalign(t));
}

static Type *base(Type *t)
{
	assert(t->nsub == 1);
	return t->sub[0];
}

static Node *add(Node *a, Node *b)
{
	Node *n;

	assert(size(a) == size(b));
	n = mkexpr(a->loc, Oadd, a, b, NULL);
	n->expr.type = a->expr.type;
	return n;
}

static Node *addk(Node *n, uvlong v)
{
	Node *k;

	k = mkintlit(n->loc, v);
	k->expr.type = exprtype(n);
	return add(n, k);
}

static Node *sub(Node *a, Node *b)
{
	Node *n;

	n = mkexpr(a->loc, Osub, a, b, NULL);
	n->expr.type = a->expr.type;
	return n;
}

static Node *subk(Node *n, uvlong v)
{
	Node *k;

	k = mkintlit(n->loc, v);
	k->expr.type = exprtype(n);
	return sub(n, k);
}

static Node *mul(Node *a, Node *b)
{
	Node *n;

	n = mkexpr(a->loc, Omul, a, b, NULL);
	n->expr.type = a->expr.type;
	return n;
}

static int addressable(Simp *s, Node *a)
{
	if (a->type == Ndecl || (a->type == Nexpr && exprop(a) == Ovar))
		return hthas(s->envoff, a) || hthas(s->stkoff, a) || hthas(s->globls, a);
	else
		return stacknode(a);
}

int stacknode(Node *n)
{
	if (n->type == Nexpr)
		return isstacktype(n->expr.type);
	else
		return isstacktype(n->decl.type);
}

int floatnode(Node *n)
{
	if (n->type == Nexpr)
		return istyfloat(n->expr.type);
	else
		return istyfloat(n->decl.type);
}

static void forcelocal(Simp *s, Node *n)
{
	assert(n->type == Ndecl || (n->type == Nexpr && exprop(n) == Ovar));
	s->stksz += size(n);
	s->stksz = align(s->stksz, min(size(n), Ptrsz));
	if (debugopt['i']) {
		dump(n, stdout);
		printf("declared at %zd, size = %zd\n", s->stksz, size(n));
	}
	htput(s->stkoff, n, itop(s->stksz));
}

static void declarelocal(Simp *s, Node *n)
{
	if (stacknode(n))
		forcelocal(s, n);
}

/* takes the address of a node, possibly converting it to
 * a pointer to the base type 'bt' */
static Node *addr(Simp *s, Node *a, Type *bt)
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

static Node *load(Node *a)
{
	Node *n;

	assert(a->expr.type->type == Typtr);
	n = mkexpr(a->loc, Oderef, a, NULL);
	n->expr.type = base(a->expr.type);
	return n;
}

static Node *deref(Node *a, Type *t)
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

static Node *set(Node *a, Node *b)
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

static void def(Simp *s, Node *var)
{
	Node *d;

	d = mkexpr(var->loc, Odef, var, NULL);
	d->expr.type = mktype(var->loc, Tyvoid);
	append(s, d);
}

static Node *disp(Srcloc loc, uint v)
{
	Node *n;

	n = mkintlit(loc, v);
	n->expr.type = tyintptr;
	return n;
}

static Node *word(Srcloc loc, uint v)
{
	Node *n;

	n = mkintlit(loc, v);
	n->expr.type = tyword;
	return n;
}

static Node *temp(Simp *simp, Node *e)
{
	Node *t, *dcl;

	assert(e->type == Nexpr);
	t = gentemp(e->loc, e->expr.type, &dcl);
	if (stacknode(e))
		declarelocal(simp, dcl);
	return t;
}

static void jmp(Simp *s, Node *lbl)
{
	append(s, mkexpr(lbl->loc, Ojmp, lbl, NULL));
}

static void cjmp(Simp *s, Node *cond, Node *iftrue, Node *iffalse)
{
	Node *jmp;

	jmp = mkexpr(cond->loc, Ocjmp, cond, iftrue, iffalse, NULL);
	append(s, jmp);
}

static Node *slicelen(Simp *s, Node *sl)
{
	/* *(&sl + sizeof(size_t)) */
	return load(addk(addr(s, sl, tyintptr), Ptrsz));
}

Node *loadvar(Simp *s, Node *n, Node *dst)
{
	Node *p, *f, *r;

	if (isconstfn(n)) {
		if (dst)
			r = dst;
		else
			r = temp(s, n);
		f = getcode(s, n);
		p = addr(s, r, exprtype(n));
		assignat(s, p, Ptrsz, f);
	} else {
		r = n;
	}
	return r;
}

static Node *seqlen(Simp *s, Node *n, Type *ty)
{
	Node *t, *r;

	if (exprtype(n)->type == Tyslice) {
		t = slicelen(s, n);
		r = simpcast(s, t, ty);
	} else if (exprtype(n)->type == Tyarray) {
		t = exprtype(n)->asize;
		r = simpcast(s, t, ty);
	} else {
		r = NULL;
	}
	return r;
}

/* if foo; bar; else baz;;
 *      => cjmp (foo) :bar :baz */
static void simpif(Simp *s, Node *n, Node *exit)
{
	Node *l1, *l2, *l3;
	Node *iftrue, *iffalse;

	l1 = genlbl(n->loc);
	l2 = genlbl(n->loc);
	if (exit)
		l3 = exit;
	else
		l3 = genlbl(n->loc);

	iftrue = n->ifstmt.iftrue;
	iffalse = n->ifstmt.iffalse;

	simpcond(s, n->ifstmt.cond, l1, l2);
	simp(s, l1);
	simp(s, iftrue);
	jmp(s, l3);
	simp(s, l2);
	/* because lots of bunched up end labels are ugly,
	 * coalesce them by handling 'elif'-like constructs
	 * separately */
	if (iffalse && iffalse->type == Nifstmt) {
		simpif(s, iffalse, exit);
	} else {
		simp(s, iffalse);
		jmp(s, l3);
	}

	if (!exit)
		simp(s, l3);
}

/* init; while cond; body;; 
 *    => init
 *       jmp :cond
 *       :body
 *           ...body...
 *           ...step...
 *       :cond
 *           ...cond...
 *            cjmp (cond) :body :end
 *       :end
 */
static void simploop(Simp *s, Node *n)
{
	Node *lbody;
	Node *lend;
	Node *lcond;
	Node *lstep;

	lbody = genlbl(n->loc);
	lcond = genlbl(n->loc);
	lstep = genlbl(n->loc);
	lend = genlbl(n->loc);

	lappend(&s->loopstep, &s->nloopstep, lstep);
	lappend(&s->loopexit, &s->nloopexit, lend);

	simp(s, n->loopstmt.init);  /* init */
	jmp(s, lcond);              /* goto test */
	simp(s, lbody);             /* body lbl */
	simp(s, n->loopstmt.body);  /* body */
	simp(s, lstep);             /* test lbl */
	simp(s, n->loopstmt.step);  /* step */
	simp(s, lcond);             /* test lbl */
	simpcond(s, n->loopstmt.cond, lbody, lend);    /* repeat? */
	simp(s, lend);              /* exit */

	s->nloopstep--;
	s->nloopexit--;
}

static void simploopmatch(Simp *s, Node *pat, Node *val, Node *ltrue, Node *lfalse)
{
	Node **cap, **out, *lload;
	size_t i, ncap, nout;

	/* pattern match */
	lload = genlbl(pat->loc);
	out = NULL;
	nout = 0;
	cap = NULL;
	ncap = 0;
	genonematch(pat, val, lload, lfalse, &out, &nout, &cap, &ncap);
	for (i = 0; i < nout; i++)
		simp(s, out[i]);
	simp(s, lload);
	for (i = 0; i < ncap; i++)
		simp(s, cap[i]);
	jmp(s, ltrue);
}

/* pat; seq; 
 *      body;;
 *
 * =>
 *      .pseudo = seqinit
 *      jmp :cond
 *      :body
 *           ...body...
 *      :step
 *           ...step...
 *      :cond
 *           ...cond...
 *           cjmp (cond) :match :end
 *      :match
 *           ...match...
 *           cjmp (match) :load :step
 *      :load
 *           matchval = load
 *      :end
 */
static void simpidxiter(Simp *s, Node *n)
{
	Node *lbody, *lstep, *lcond, *lmatch, *lend;
	Node *idx, *len, *dcl, *seq, *val, *done;
	Node *zero;

	lbody = genlbl(n->loc);
	lstep = genlbl(n->loc);
	lcond = genlbl(n->loc);
	lmatch = genlbl(n->loc);
	lend = genlbl(n->loc);

	lappend(&s->loopstep, &s->nloopstep, lstep);
	lappend(&s->loopexit, &s->nloopexit, lend);

	zero = mkintlit(n->loc, 0);
	zero->expr.type = tyintptr;

	seq = rval(s, n->iterstmt.seq, NULL);
	idx = gentemp(n->loc, tyintptr, &dcl);
	declarelocal(s, dcl);

	/* setup */
	append(s, assign(s, idx, zero));
	jmp(s, lcond);
	simp(s, lbody);

	/* body */
	simp(s, n->iterstmt.body);
	/* step */
	simp(s, lstep);
	simp(s, assign(s, idx, addk(idx, 1)));
	/* condition */
	simp(s, lcond);
	len = seqlen(s, seq, tyintptr);
	done = mkexpr(n->loc, Olt, idx, len, NULL);
	cjmp(s, done, lmatch, lend);
	simp(s, lmatch);
	val = load(idxaddr(s, seq, idx));

	/* pattern match */
	simploopmatch(s, n->iterstmt.elt, val, lbody, lstep);
	jmp(s, lbody);
	simp(s, lend);

	s->nloopstep--;
	s->nloopexit--;
}

static Node *itertraitfn(Srcloc loc, Trait *tr, char *fn, Type *ty)
{
	Node *proto, *dcl, *var;
	char *name;
	size_t i;

	for (i = 0; i < tr->nfuncs; i++) {
		name = declname(tr->funcs[i]);
		if (!strcmp(fn, name)) {
			proto = tr->funcs[i];
			dcl = htget(proto->decl.impls, ty);
			var = mkexpr(loc, Ovar, dcl->decl.name, NULL);
			var->expr.type = codetype(dcl->decl.type);
			var->expr.did = dcl->decl.did;
			return var;
		}
	}
	return NULL;
}

/* for pat in seq
 * 	body;;
 * =>
 * 	.seq = seq
 * 	.elt = elt
 * 	:body
 * 		..body..
 * 	:step
 * 		__iterfin__(&seq, &elt)
 * 		cond = __iternext__(&seq, &eltout)
 * 		cjmp (cond) :match :end
 * 	:match
 * 		...match...
 * 		cjmp (match) :load :step
 * 	:load
 * 		...load matches...
 * 	:end
 */
static void simptraititer(Simp *s, Node *n)
{
	Node *lbody, *lclean, *lstep, *lmatch, *lend;
	Node *done, *val, *iter, *valptr, *iterptr;
	Node *func, *call, *asn;
	Trait *tr;

	val = temp(s, n->iterstmt.elt);
	valptr = mkexpr(val->loc, Oaddr, val, NULL);
	valptr->expr.type = mktyptr(n->loc, exprtype(val));
	iter = temp(s, n->iterstmt.seq);
	iterptr = mkexpr(val->loc, Oaddr, iter, NULL);
	iterptr->expr.type = mktyptr(n->loc, exprtype(iter));
	tr = traittab[Tciter];

	/* create labels */
	lbody = genlbl(n->loc);
	lclean = genlbl(n->loc);
	lstep = genlbl(n->loc);
	lmatch = genlbl(n->loc);
	lend = genlbl(n->loc);
	lappend(&s->loopstep, &s->nloopstep, lstep);
	lappend(&s->loopexit, &s->nloopexit, lend);

	asn = assign(s, iter, n->iterstmt.seq);
	append(s, asn);
	jmp(s, lstep);
	simp(s, lbody);
	/* body */
	simp(s, n->iterstmt.body);
	simp(s, lclean);

	/* call iterator cleanup */
	func = itertraitfn(n->loc, tr, "__iterfin__", exprtype(iter));
	call = mkexpr(n->loc, Ocall, func, iterptr, valptr, NULL);
	call->expr.type = mktype(n->loc, Tyvoid);
	append(s, call);

	simp(s, lstep);
	/* call iterator step */
	func = itertraitfn(n->loc, tr, "__iternext__", exprtype(iter));
	call = mkexpr(n->loc, Ocall, func, iterptr, valptr, NULL);
	done = gentemp(n->loc, mktype(n->loc, Tybool), NULL);
	call->expr.type = exprtype(done);
	asn = assign(s, done, call);
	append(s, asn);
	cjmp(s, done, lmatch, lend);

	/* pattern match */
	simp(s, lmatch);
	simploopmatch(s, n->iterstmt.elt, val, lbody, lclean);
	jmp(s, lbody);
	simp(s, lend);

	s->nloopstep--;
	s->nloopexit--;
}

static void simpiter(Simp *s, Node *n)
{
	switch (tybase(exprtype(n->iterstmt.seq))->type) {
	case Tyarray:	simpidxiter(s, n);	break;
	case Tyslice:	simpidxiter(s, n);	break;
	default:	simptraititer(s, n);	break;
	}
}

static Node *uconid(Simp *s, Node *n)
{
	Ucon *uc;

	n = rval(s, n, NULL);
	if (exprop(n) != Oucon)
		return load(addr(s, n, mktype(n->loc, Tyuint)));

	uc = finducon(exprtype(n), n->expr.args[0]);
	return word(uc->loc, uc->id);
}

static void simpblk(Simp *s, Node *n)
{
	size_t i;

	for (i = 0; i < n->block.nstmts; i++) {
		n->block.stmts[i] = fold(n->block.stmts[i], 0);
		simp(s, n->block.stmts[i]);
	}
}

static Node *geninitdecl(Node *init, Type *ty, Node **dcl)
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

static Node *simpcode(Simp *s, Node *fn)
{
	Node *r, *d;

	r = geninitdecl(fn, codetype(exprtype(fn)), &d);
	htput(s->globls, d, asmname(d));
	lappend(&file->file.stmts, &file->file.nstmts, d);
	return r;
}

static Node *simpblob(Simp *s, Node *blob)
{
	Node *r, *d;

	r = geninitdecl(blob, exprtype(blob), &d);
	htput(s->globls, d, asmname(d));
	lappend(&s->blobs, &s->nblobs, d);
	return r;
}

static Node *ptrsized(Simp *s, Node *v)
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

static Node *membaddr(Simp *s, Node *n)
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

static void checkidx(Simp *s, Node *len, Node *idx)
{
	Node *cmp, *die;
	Node *ok, *fail;

	if (!len)
		return;
	/* create expressions */
	cmp = mkexpr(idx->loc, Olt, ptrsized(s, idx), ptrsized(s, len), NULL);
	cmp->expr.type = mktype(len->loc, Tybool);
	ok = genlbl(len->loc);
	fail = genlbl(len->loc);
	die = mkexpr(idx->loc, Ocall, abortoob, NULL);
	die->expr.type = mktype(len->loc, Tyvoid);

	/* insert them */
	cjmp(s, cmp, ok, fail);
	append(s, fail);
	append(s, die);
	append(s, ok);
}

static Node *idxaddr(Simp *s, Node *seq, Node *idx)
{
	Node *a, *t, *u, *v, *w; /* temps */
	Node *r; /* result */
	Type *ty;
	size_t sz;

	a = rval(s, seq, NULL);
	ty = exprtype(seq)->sub[0];
	if (exprtype(seq)->type == Tyarray) {
		t = addr(s, a, ty);
		w = exprtype(a)->asize;
	} else if (seq->expr.type->type == Tyslice) {
		t = load(addr(s, a, mktyptr(seq->loc, ty)));
		w = slicelen(s, a);
	} else {
		die("Can't index type %s\n", tystr(seq->expr.type));
	}
	assert(t->expr.type->type == Typtr);
	u = rval(s, idx, NULL);
	u = ptrsized(s, u);
	checkidx(s, w, u);
	sz = tysize(ty);
	v = mul(u, disp(seq->loc, sz));
	r = add(t, v);
	return r;
}

static Node *slicebase(Simp *s, Node *n, Node *off)
{
	Node *u, *v;
	Type *ty;
	int sz;

	u = NULL;
	ty = tybase(exprtype(n));
	switch (ty->type) {
	case Typtr:	u = n;	break;
	case Tyarray:	u = addr(s, n, base(exprtype(n)));	break;
	case Tyslice:	u = load(addr(s, n, mktyptr(n->loc, base(exprtype(n)))));	break;
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

static Node *loadidx(Simp *s, Node *arr, Node *idx)
{
	Node *v, *a;

	a = rval(s, arr, NULL);
	lappend(&s->idxctx, &s->nidxctx, a);
	v = deref(idxaddr(s, a, idx), NULL);
	lpop(&s->idxctx, &s->nidxctx);
	return v;
}

static Node *lval(Simp *s, Node *n)
{
	Node *r;
	Node **args;

	args = n->expr.args;
	switch (exprop(n)) {
	case Ovar:	r = loadvar(s, n, NULL);	break;
	case Oidx:	r = loadidx(s, args[0], args[1]);	break;
	case Oderef:	r = deref(rval(s, args[0], NULL), NULL);	break;
	case Omemb:	r = rval(s, n, NULL);	break;
	case Ostruct:	r = rval(s, n, NULL);	break;
	case Oucon:	r = rval(s, n, NULL);	break;
	case Oarr:	r = rval(s, n, NULL);	break;
	case Ogap:	r = temp(s, n);	break;

			/* not actually expressible as lvalues in syntax, but we generate them */
	case Oudata:	r = rval(s, n, NULL);	break;
	case Outag:	r = rval(s, n, NULL);	break;
	case Otupget:	r = rval(s, n, NULL);	break;
	default:
			fatal(n, "%s cannot be an lvalue", opstr[exprop(n)]);
			break;
	}
	return r;
}

static void simpcond(Simp *s, Node *n, Node *ltrue, Node *lfalse)
{
	Node **args;
	Node *v, *lnext;

	args = n->expr.args;
	switch (exprop(n)) {
	case Oland:
		lnext = genlbl(n->loc);
		simpcond(s, args[0], lnext, lfalse);
		append(s, lnext);
		simpcond(s, args[1], ltrue, lfalse);
		break;
	case Olor:
		lnext = genlbl(n->loc);
		simpcond(s, args[0], ltrue, lnext);
		append(s, lnext);
		simpcond(s, args[1], ltrue, lfalse);
		break;
	case Olnot:
		simpcond(s, args[0], lfalse, ltrue);
		break;
	default:
		v = rval(s, n, NULL);
		cjmp(s, v, ltrue, lfalse);
		break;
	}
}

static Node *intconvert(Simp *s, Node *from, Type *to, int issigned)
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

static Node *simpcast(Simp *s, Node *val, Type *to)
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
		t = tybase(exprtype(val));
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
static Node *simpslice(Simp *s, Node *n, Node *dst)
{
	Node *t;
	Node *start, *end;
	Node *seq, *base, *sz, *len;
	Node *stbase, *stlen;

	if (dst)
		t = dst;
	else
		t = temp(s, n);
	seq = rval(s, n->expr.args[0], NULL);
	if (tybase(exprtype(seq))->type != Typtr)
		lappend(&s->idxctx, &s->nidxctx, seq);
	/* *(&slice) = (void*)base + off*sz */
	base = slicebase(s, seq, n->expr.args[1]);
	start = ptrsized(s, rval(s, n->expr.args[1], NULL));
	end = ptrsized(s, rval(s, n->expr.args[2], NULL));
	len = sub(end, start);
	/* we can be storing through a pointer, in the case
	 * of '*foo = bar'. */
	if (tybase(exprtype(t))->type == Typtr) {
		stbase = set(simpcast(s, t, mktyptr(t->loc, tyintptr)), base);
		sz = addk(simpcast(s, t, mktyptr(t->loc, tyintptr)), Ptrsz);
	} else {
		stbase = set(deref(addr(s, t, tyintptr), NULL), base);
		sz = addk(addr(s, t, tyintptr), Ptrsz);
	}
	if (tybase(exprtype(seq))->type != Typtr)
		lpop(&s->idxctx, &s->nidxctx);
	/* *(&slice + ptrsz) = len */
	stlen = set(deref(sz, NULL), len);
	append(s, stbase);
	append(s, stlen);
	return t;
}

static Node *visit(Simp *s, Node *n)
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

static Node *tupget(Simp *s, Node *tup, size_t idx, Node *dst)
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

/* Takes a tuple and binds the i'th element of it to the
 * i'th name on the rhs of the assignment. */
static Node *destructure(Simp *s, Node *lhs, Node *rhs)
{
	Node *lv, *rv, **args;
	size_t i;

	args = lhs->expr.args;
	rhs = rval(s, rhs, NULL);
	for (i = 0; i < lhs->expr.nargs; i++) {
		lv = lval(s, args[i]);
		rv = tupget(s, rhs, i, lv);
		assert(rv == lv);
	}

	return NULL;
}

static Node *assign(Simp *s, Node *lhs, Node *rhs)
{
	Node *t, *u, *v, *r;

	if (exprop(lhs) == Otup) {
		r = destructure(s, lhs, rhs);
	} else {
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
		} else {
			r = set(t, u);
		}
	}
	return r;
}

static Node *assignat(Simp *s, Node *r, size_t off, Node *val)
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
static Node *simptup(Simp *s, Node *n, Node *dst)
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

static Node *simpucon(Simp *s, Node *n, Node *dst)
{
	Node *tmp, *u, *tag, *elt, *sz;
	Node *r;
	Type *ty;
	Ucon *uc;
	size_t i, o;

	/* find the ucon we're constructing here */
	ty = tybase(n->expr.type);
	uc = NULL;
	for (i = 0; i < ty->nmemb; i++) {
		if (!strcmp(namestr(n->expr.args[0]), namestr(ty->udecls[i]->name))) {
			uc = ty->udecls[i];
			break;
		}
	}
	if (!uc)
		die("Couldn't find union constructor");

	if (dst)
		tmp = dst;
	else
		tmp = temp(s, n);

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

static Node *simpuget(Simp *s, Node *n, Node *dst)
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

/* simplifies 
 *      a || b
 * to
 *      if a || b
 *              t = true
 *      else
 *              t = false
 *      ;;
 */
static Node *simplazy(Simp *s, Node *n)
{
	Node *r, *t, *u;
	Node *ltrue, *lfalse, *ldone;

	/* set up temps and labels */
	r = temp(s, n);
	ltrue = genlbl(n->loc);
	lfalse = genlbl(n->loc);
	ldone = genlbl(n->loc);

	/* simp the conditional */
	simpcond(s, n, ltrue, lfalse);

	/* if true */
	append(s, ltrue);
	u = mkexpr(n->loc, Olit, mkbool(n->loc, 1), NULL);
	u->expr.type = mktype(n->loc, Tybool);
	t = set(r, u);
	append(s, t);
	jmp(s, ldone);

	/* if false */
	append(s, lfalse);
	u = mkexpr(n->loc, Olit, mkbool(n->loc, 0), NULL);
	u->expr.type = mktype(n->loc, Tybool);
	t = set(r, u);
	append(s, t);
	jmp(s, ldone);

	/* finish */
	append(s, ldone);
	return r;
}

static Node *comparecomplex(Simp *s, Node *n, Op op)
{
	fatal(n, "Complex comparisons not yet supported\n");
	return NULL;
}

static Node *compare(Simp *s, Node *n, int fields)
{
	const Op cmpmap[Numops][3] = {
		[Oeq] = {Oeq, Oueq, Ofeq},
		[One] = {One, Oune, Ofne},
		[Ogt] = {Ogt, Ougt, Ofgt},
		[Oge] = {Oge, Ouge, Ofge},
		[Olt] = {Olt, Oult, Oflt},
		[Ole] = {Ole, Oule, Ofle}
	};
	Node *r;
	Op newop;

	/* void is always void */
	if (tybase(exprtype(n->expr.args[0]))->type == Tyvoid)
		return mkboollit(n->loc, 1);

	newop = Obad;
	if (istysigned(tybase(exprtype(n->expr.args[0]))))
		newop = cmpmap[n->expr.op][0];
	else if (istyunsigned(tybase(exprtype(n->expr.args[0]))))
		newop = cmpmap[n->expr.op][1];
	else if (istyfloat(tybase(exprtype(n->expr.args[0]))))
		newop = cmpmap[n->expr.op][2];

	if (newop != Obad) {
		n->expr.op = newop;
		r = visit(s, n);
	} else if (fields) {
		r = comparecomplex(s, n, exprop(n));
	} else {
		fatal(n, "unsupported comparison on values");
	}
	return r;
}

static Node *vatypeinfo(Simp *s, Node *n)
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
		exprtype(n->expr.args[i])->isreflect = 1;
		lappend(&st, &nst, exprtype(n->expr.args[i]));
	}
	vt = mktytuple(n->loc, st, nst);
	vt->isreflect = 1;

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

static Node *capture(Simp *s, Node *n, Node *dst)
{
	Node *fn, *t, *f, *e, *val, *dcl, *fp, *envsz;
	size_t nenv, nenvt, off, i;
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

	env = getclosure(fn->func.scope, &nenv);
	if (env) {
		/* we need these in a deterministic order so that we can
		   put them in the right place both when we use them and
		   when we capture them.  */
		qsort(env, nenv, sizeof(Node*), envcmp);

		/* make the tuple that will hold the environment */
		envt = NULL;
		nenvt = 0;
		/* reserve space for size */
		lappend(&envt, &nenvt, tyintptr);
		for (i = 0; i < nenv; i++)
			lappend(&envt, &nenvt, decltype(env[i]));

		t = gentemp(n->loc, mktytuple(n->loc, envt, nenvt), &dcl);
		forcelocal(s, dcl);
		e = addr(s, t, exprtype(t));

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
		envsz = mkintlit(n->loc, off);
		envsz->expr.type = tyintptr;
		assignat(s, e, 0, envsz);
		assignat(s, fp, 0, e);
	}
	assignat(s, fp, Ptrsz, f);
	return dst;
}

static Node *getenvptr(Simp *s, Node *n)
{
	assert(tybase(exprtype(n))->type == Tyfunc);
	return load(addr(s, n, tyintptr));
}

static Node *getcode(Simp *s, Node *n)
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

static Node *simpcall(Simp *s, Node *n, Node *dst)
{
	Node *r, *call, *fn;
	size_t i, nargs;
	Node **args;
	Type *ft;
	Op op;

	/* NB: If we called rval() on a const function, , we would end up with
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

static Node *rval(Simp *s, Node *n, Node *dst)
{
	Node *t, *u, *v; /* temporary nodes */
	Node *r; /* expression result */
	Node **args;
	size_t i;
	Type *ty;

	const Op fusedmap[Numops] = {
		[Oaddeq]	= Oadd,
		[Osubeq]	= Osub,
		[Omuleq]	= Omul,
		[Odiveq]	= Odiv,
		[Omodeq]	= Omod,
		[Oboreq]	= Obor,
		[Obandeq]	= Oband,
		[Obxoreq]	= Obxor,
		[Obsleq]	= Obsl,
		[Obsreq]	= Obsr,
	};

	r = NULL;
	args = n->expr.args;
	switch (exprop(n)) {
	case Olor: case Oland:
		r = simplazy(s, n);
		break;
	case Osize:
		r = mkintlit(n->loc, size(args[0]));
		r->expr.type = exprtype(n);
		break;
	case Oslice:
		r = simpslice(s, n, dst);
		break;
	case Oidx:
		t = rval(s, n->expr.args[0], NULL);
		lappend(&s->idxctx, &s->nidxctx, t);
		u = idxaddr(s, t, n->expr.args[1]);
		lpop(&s->idxctx, &s->nidxctx);
		r = load(u);
		break;
		/* array.len slice.len are magic 'virtual' members.
		 * they need to be special cased. */
	case Omemb:
		if (exprtype(args[0])->type == Tyslice || exprtype(args[0])->type == Tyarray) {
			r = seqlen(s, args[0], exprtype(n));
		} else {
			t = membaddr(s, n);
			r = load(t);
		}
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
		t = addr(s, dst, exprtype(dst));
		for (i = 0; i < n->expr.nargs; i++)
			assignat(s, t, size(n->expr.args[i])*i, rval(s, n->expr.args[i], NULL));
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
			assignat(s, t, offset(n, n->expr.args[i]->expr.idx), rval(s, n->expr.args[i], NULL));
		r = dst;
		break;
	case Ocast:
		r = simpcast(s, args[0], exprtype(n));
		break;

		/* fused ops:
		 * foo ?= blah
		 *    =>
		 *     foo = foo ? blah*/
	case Oaddeq: case Osubeq: case Omuleq: case Odiveq: case Omodeq:
	case Oboreq: case Obandeq: case Obxoreq: case Obsleq: case Obsreq:
		assert(fusedmap[exprop(n)] != Obad);
		u = lval(s, args[0]);
		v = rval(s, args[1], NULL);
		v = mkexpr(n->loc, fusedmap[exprop(n)], u, v, NULL);
		v->expr.type = u->expr.type;
		r = set(u, v);
		break;

		/* ++expr(x)
		 *  => args[0] = args[0] + 1
		 *     expr(x) */
	case Opreinc:
		v = assign(s, args[0], addk(args[0], 1));
		append(s, v);
		r = rval(s, args[0], NULL);
		break;
	case Opredec:
		v = assign(s, args[0], subk(args[0], 1));
		append(s, v);
		r = rval(s, args[0], NULL);
		break;

		/* expr(x++)
		 *   => expr
		 *      x = x + 1
		 */
	case Opostinc:
		r = lval(s, args[0]);
		t = assign(s, r, addk(r, 1));
		lappend(&s->incqueue, &s->nqueue, t);
		break;
	case Opostdec:
		r = lval(s, args[0]);
		t = assign(s, r, subk(r, 1));
		lappend(&s->incqueue, &s->nqueue, t);
		break;
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
	case Oidxlen:
		if (s->nidxctx == 0)
			fatal(n, "'$' undefined outside of index or slice expression");
		return seqlen(s, s->idxctx[s->nidxctx - 1], exprtype(n));
		break;
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
		} else if (n->expr.nargs && n->expr.args[0]) {
			t = s->ret;
			/* void calls return nothing */
			if (t) {
				t = set(t, rval(s, args[0], NULL));
				append(s, t);
			}
		}
		/* drain the increment queue before we return */
		for (i = 0; i < s->nqueue; i++)
			append(s, s->incqueue[i]);
		lfree(&s->incqueue, &s->nqueue);
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
			t =mkfloat(n->loc, -1.0); 
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
	case Obreak:
		if (s->nloopexit == 0)
			fatal(n, "trying to break when not in loop");
		jmp(s, s->loopexit[s->nloopexit - 1]);
		break;
	case Ocontinue:
		if (s->nloopstep == 0)
			fatal(n, "trying to continue when not in loop");
		jmp(s, s->loopstep[s->nloopstep - 1]);
		break;
	case Oeq: case One:
		r = compare(s, n, 1);
		break;
	case Ogt: case Oge: case Olt: case Ole:
		r = compare(s, n, 0);
		break;
	case Otupget:
		assert(exprop(args[1]) == Olit);
		i = args[1]->expr.args[0]->lit.intval;
		t = rval(s, args[0], NULL);
		r = tupget(s, t, i, dst);
		break;
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

static void declarearg(Simp *s, Node *n)
{
	assert(n->type == Ndecl || (n->type == Nexpr && exprop(n) == Ovar));
	lappend(&s->args, &s->nargs, n);
}

static int islbl(Node *n)
{
	Node *l;
	if (exprop(n) != Olit)
		return 0;
	l = n->expr.args[0];
	return l->type == Nlit && l->lit.littype == Llbl;
}

static void simpmatch(Simp *s, Node *n)
{
	Node *val;
	Node **match;
	size_t i, nmatch;

	val = rval(s, n->matchstmt.val, NULL);

	match = NULL;
	nmatch = 0;
	genmatch(n, val, &match, &nmatch);
	for (i = 0; i < nmatch; i++)
		simp(s, match[i]);
}

static Node *simp(Simp *s, Node *n)
{
	Node *r, *t, *u;
	size_t i;

	if (!n)
		return NULL;
	r = NULL;
	switch (n->type) {
	case Nblock:	simpblk(s, n);	break;
	case Nloopstmt:	simploop(s, n);	break;
	case Niterstmt:	simpiter(s, n);	break;
	case Nifstmt:	simpif(s, n, NULL);	break;
	case Nmatchstmt:	simpmatch(s, n);	break;
	case Nexpr:
		if (islbl(n))
			append(s, n);
		else
			r = rval(s, n, NULL);
		if (r)
			append(s, r);
		/* drain the increment queue for this expr */
		for (i = 0; i < s->nqueue; i++)
			append(s, s->incqueue[i]);
		lfree(&s->incqueue, &s->nqueue);
		break;

	case Ndecl:
		declarelocal(s, n);
		t = mkexpr(n->loc, Ovar, n->decl.name, NULL);
		if (n->decl.init) {
			u = mkexpr(n->loc, Oasn, t, n->decl.init, NULL);
			u->expr.type = n->decl.type;
			t->expr.type = n->decl.type;
			t->expr.did = n->decl.did;
			simp(s, u);
		}
		break;
	default:
		dump(n, stderr);
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
static void flatten(Simp *s, Node *f)
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
	} else if (ty->type != Tyvoid) {
		s->isbigret = 0;
		s->ret = gentemp(f->loc, ty, &dcl);
	}

	for (i = 0; i < f->func.nargs; i++) {
		declarearg(s, f->func.args[i]);
	}
	simp(s, f->func.body);

	append(s, s->endlbl);
}

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

static int envcmp(const void *pa, const void *pb)
{
	const Node *a, *b;

	a = *(const Node**)pa;
	b = *(const Node**)pb;
	return b->decl.did - a->decl.did;
}

static void collectenv(Simp *s, Node *fn)
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

static Func *simpfn(Simp *s, char *name, Node *dcl)
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
	flatten(s, n);

	if (debugopt['f'] || debugopt['F'])
		for (i = 0; i < s->nstmts; i++)
			dump(s->stmts[i], stdout);
	for (i = 0; i < s->nstmts; i++) {
		if (s->stmts[i]->type != Nexpr)
			continue;
		if (debugopt['f']) {
			printf("FOLD FROM ----------\n");
			dump(s->stmts[i], stdout);
		}
		s->stmts[i] = fold(s->stmts[i], 0);
		if (debugopt['f']) {
			printf("TO ------------\n");
			dump(s->stmts[i], stdout);
			printf("DONE ----------------\n");
		}
	}

	cfg = mkcfg(dcl, s->stmts, s->nstmts);
	check(cfg);
	if (debugopt['t'] || debugopt['s'])
		dumpcfg(cfg, stdout);

	fn = zalloc(sizeof(Func));
	fn->name = strdup(name);
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

static void extractsub(Simp *s, Node *e)
{
	size_t i;

	assert(e != NULL);
	switch (exprop(e)) {
	case Oslice:
		if (exprop(e->expr.args[0]) == Oarr)
			e->expr.args[0] = simpblob(s, e->expr.args[0]);
		break;
	case Oarr:
	case Ostruct:
		for (i = 0; i < e->expr.nargs; i++)
			extractsub(s, e->expr.args[i]);
		break;
	default:
		break;
	}
}

static void simpconstinit(Simp *s, Node *dcl)
{
	Node *e;

	dcl->decl.init = fold(dcl->decl.init, 1);;
	e = dcl->decl.init;
	if (e && exprop(e) == Olit) {
		if (e->expr.args[0]->lit.littype == Lfunc)
			simpcode(s, e);
		else
			lappend(&s->blobs, &s->nblobs, dcl);
	} else if (dcl->decl.isconst) {
		switch (exprop(e)) {
		case Oarr:
		case Ostruct:
		case Oslice:
			extractsub(s, e);
			lappend(&s->blobs, &s->nblobs, dcl);
			break;
		default:
			fatal(dcl, "unsupported initializer for %s", declname(dcl));
			break;
		}
	} else if (!dcl->decl.isconst && !e) {
		lappend(&s->blobs, &s->nblobs, dcl);
	} else {
		die("Non-constant initializer for %s\n", declname(dcl));
	}
}

int ismain(Node *dcl)
{
	Node *n;

	n = dcl->decl.name;
	if (n->name.ns)
		return 0;
	return strcmp(n->name.name, "main") == 0;
}

void simpglobl(Node *dcl, Htab *globls, Func ***fn, size_t *nfn, Node ***blob, size_t *nblob)
{
	Simp s = {0,};
	char *name;
	Func *f;

	if (ismain(dcl))
		dcl->decl.vis = Vishidden;
	s.stkoff = mkht(varhash, vareq);
	s.envoff = mkht(varhash, vareq);
	s.globls = globls;
	s.blobs = *blob;
	s.nblobs = *nblob;
	s.hasenv = 0;
	name = asmname(dcl);

	if (dcl->decl.isextern || dcl->decl.isgeneric)
		return;
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
