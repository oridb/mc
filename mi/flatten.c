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
#include "../config.h"


/* takes a list of nodes, and reduces it (and it's subnodes) to a list
 * following these constraints:
 *      - All nodes are expression nodes
 *      - Nodes with side effects are root nodes
 *      - All nodes operate on machine-primitive types and tuples
 */
typedef struct Flattenctx Flattenctx;
struct Flattenctx {
	int isglobl;

	/* return handling */
	Node **stmts;
	size_t nstmts;

	/* return handling */
	int hasenv;
	int isbigret;

	/* pre/postinc handling */
	Node **incqueue;
	size_t nqueue;

	/* break/continue handling */
	Node **loopstep;
	size_t nloopstep;
	Node **loopexit;
	size_t nloopexit;

	/* location handling */
	Htab *globls;

	size_t stksz;
};

static Node *flatten(Flattenctx *s, Node *n);
static Node *rval(Flattenctx *s, Node *n);
static Node *lval(Flattenctx *s, Node *n);

static void append(Flattenctx *s, Node *n)
{
	if (debugopt['F'])
		dump(n, stdout);
	lappend(&s->stmts, &s->nstmts, n);
}

static void cjmp(Flattenctx *s, Node *cond, Node *iftrue, Node *iffalse)
{
	Node *jmp;

	jmp = mkexpr(cond->loc, Ocjmp, cond, iftrue, iffalse, NULL);
	jmp->expr.type = mktype(cond->loc, Tyvoid);
	append(s, jmp);
}

static void jmp(Flattenctx *s, Node *lbl)
{
	Node *n;

	n = mkexpr(lbl->loc, Ojmp, lbl, NULL);
	n->expr.type = mktype(n->loc, Tyvoid);
	append(s, n);
}

static Node *asn(Node *a, Node *b)
{
	Node *n;

	assert(a != NULL && b != NULL);
	if (tybase(exprtype(a))->type == Tyvoid)
		return a;

	n = mkexpr(a->loc, Oasn, a, b, NULL);
	n->expr.type = exprtype(a);
	return n;
}

static int islbl(Node *n)
{
	Node *l;
	if (exprop(n) != Olit)
		return 0;
	l = n->expr.args[0];
	return l->type == Nlit && l->lit.littype == Llbl;
}

static Node *temp(Flattenctx *flatten, Node *e)
{
	Node *t, *dcl;

	assert(e->type == Nexpr);
	t = gentemp(e->loc, e->expr.type, &dcl);
	return t;
}

static Node *add(Node *a, Node *b)
{
	Node *n;

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

static Node *seqlen(Flattenctx *s, Node *n, Type *ty)
{
	Node *r;

	if (!ty)
		ty = n->expr.type;
	if (exprtype(n)->type == Tyarray) {
		r = mkexpr(n->loc, Osllen, rval(s, n), NULL);
		r->expr.type = ty;
	} else if (exprtype(n)->type == Tyslice) {
		r = mkexpr(n->loc, Osllen, rval(s, n), NULL);
		r->expr.type = ty;
	}
	assert(r != NULL);
	return r;
}

static Node *visit(Flattenctx *s, Node *n)
{
	size_t i;
	Node *r;

	for (i = 0; i < n->expr.nargs; i++)
		n->expr.args[i] = rval(s, n->expr.args[i]);
	if (opispure[exprop(n)]) {
		r = n;
	} else {
		if (exprtype(n)->type == Tyvoid) {
			r = mkexpr(n->loc, Olit, mkvoid(n->loc), NULL);
			r->expr.type = mktype(n->loc, Tyvoid);
			append(s, n);
		} else {
			r = temp(s, n);
			append(s, asn(r, n));
		}
	}
	return r;
}

static void flattencond(Flattenctx *s, Node *n, Node *ltrue, Node *lfalse)
{
	Node **args;
	Node *v, *lnext;

	args = n->expr.args;
	switch (exprop(n)) {
	case Oland:
		lnext = genlbl(n->loc);
		flattencond(s, args[0], lnext, lfalse);
		append(s, lnext);
		flattencond(s, args[1], ltrue, lfalse);
		break;
	case Olor:
		lnext = genlbl(n->loc);
		flattencond(s, args[0], ltrue, lnext);
		append(s, lnext);
		flattencond(s, args[1], ltrue, lfalse);
		break;
	case Olnot:
		flattencond(s, args[0], lfalse, ltrue);
		break;
	default:
		v = rval(s, n);
		cjmp(s, v, ltrue, lfalse);
		break;
	}
}

/* flattenlifies 
 *      a || b
 * to
 *      if a || b
 *              t = true
 *      else
 *              t = false
 *      ;;
 */
static Node *flattenlazy(Flattenctx *s, Node *n)
{
	Node *r, *t, *u;
	Node *ltrue, *lfalse, *ldone;

	/* set up temps and labels */
	r = temp(s, n);
	ltrue = genlbl(n->loc);
	lfalse = genlbl(n->loc);
	ldone = genlbl(n->loc);

	/* flatten the conditional */
	flattencond(s, n, ltrue, lfalse);

	/* if true */
	append(s, ltrue);
	u = mkexpr(n->loc, Olit, mkbool(n->loc, 1), NULL);
	u->expr.type = mktype(n->loc, Tybool);
	t = asn(r, u);
	append(s, t);
	jmp(s, ldone);

	/* if false */
	append(s, lfalse);
	u = mkexpr(n->loc, Olit, mkbool(n->loc, 0), NULL);
	u->expr.type = mktype(n->loc, Tybool);
	t = asn(r, u);
	append(s, t);
	jmp(s, ldone);

	/* finish */
	append(s, ldone);
	return r;
}

static Node *destructure(Flattenctx *s, Node *lhs, Node *rhs)
{
	Node *lv, *rv, *idx;
	Node **args;
	size_t i;

	args = lhs->expr.args;
	rhs = rval(s, rhs);
	for (i = 0; i < lhs->expr.nargs; i++) {
		lv = lval(s, args[i]);

		idx = mkintlit(rhs->loc, i);
		idx->expr.type = mktype(rhs->loc, Tyuint64);
		rv = mkexpr(rhs->loc, Otupget, rhs, idx, NULL);
		rv->expr.type = lhs->expr.type;

		append(s, asn(lv, rv));
	}
	return rhs;
}

static Node *comparecomplex(Flattenctx *s, Node *n, Op op)
{
	fatal(n, "Complex comparisons not yet supported\n");
	return NULL;
}

static Node *compare(Flattenctx *s, Node *n, int fields)
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
	Type *ty;
	Op newop;

	/* void is always void */
	if (tybase(exprtype(n->expr.args[0]))->type == Tyvoid)
		return mkboollit(n->loc, 1);

	newop = Obad;
	ty = tybase(exprtype(n->expr.args[0]));
	if (istysigned(ty))
		newop = cmpmap[n->expr.op][0];
	else if (istyunsigned(ty))
		newop = cmpmap[n->expr.op][1];
	else if (istyunsigned(ty))
		newop = cmpmap[n->expr.op][1];
	else if (ty->type == Typtr)
		newop = cmpmap[n->expr.op][1];
	else if (istyfloat(ty))
		newop = cmpmap[n->expr.op][2];

	if (newop != Obad) {
		n->expr.op = newop;
		r = visit(s, n);
	} else if (fields) {
		r = comparecomplex(s, n, exprop(n));
	} else {
		fatal(n, "unsupported comparison on values");
	}
	r->expr.type = mktype(n->loc, Tybool);
	return r;
}
static Node *rval(Flattenctx *s, Node *n)
{
	Node *t, *u; /* temporary nodes */
	Node *r; /* expression result */
	Node **args;
	size_t i;
	Type *ty;

//	const Op fusedmap[Numops] = {
//		[Oaddeq]	= Oadd,
//		[Osubeq]	= Osub,
//		[Omuleq]	= Omul,
//		[Odiveq]	= Odiv,
//		[Omodeq]	= Omod,
//		[Oboreq]	= Obor,
//		[Obandeq]	= Oband,
//		[Obxoreq]	= Obxor,
//		[Obsleq]	= Obsl,
//		[Obsreq]	= Obsr,
//	};

	r = NULL;
	args = n->expr.args;
	switch (exprop(n)) {
	case Osize:
		r = n;	/* don't touch subexprs; they're a pseudo decl */
		break;
	case Olor: case Oland:
		r = flattenlazy(s, n);
		break;
	case Oidx:
		t = rval(s, n->expr.args[0]);
		u = rval(s, n->expr.args[1]);
		r = mkexpr(n->loc, Oidx, t, u, NULL);
		r->expr.type = n->expr.type;
		break;
		/* array.len slice.len are magic 'virtual' members.
		 * they need to be special cased. */
	case Omemb:
                ty = tybase(exprtype(args[0]));
		if (ty->type == Tyslice || ty->type == Tyarray) {
			r = seqlen(s, args[0], exprtype(n));
		} else {
			t = rval(s, args[0]);
			r = mkexpr(n->loc, Omemb, t, args[1], NULL);
			r->expr.type = n->expr.type;
		}
		break;
	case Oucon:
		if (n->expr.nargs > 1)
			t = rval(s, args[1]);
		else
			t = NULL;
		r = mkexpr(n->loc, Oucon, args[0], t, NULL);
		r->expr.type = n->expr.type;
		break;
//	case Outag:
//		r = uconid(s, args[0]);
//		break;
//	case Oudata:
//		r = flattenuget(s, n, dst);
//		break;
//	case Otup:
//		r = flattentup(s, n, dst);
//		break;
//	case Oarr:
//		if (!dst)
//			dst = temp(s, n);
//		t = addr(s, dst, exprtype(dst));
//		for (i = 0; i < n->expr.nargs; i++)
//			assignat(s, t, size(n->expr.args[i])*i, rval(s, n->expr.args[i], NULL));
//		r = dst;
//		break;
//	case Ostruct:
//		if (!dst)
//		dst = temp(s, n);
//		u = mkexpr(dst->loc, Odef, dst, NULL);
//		u->expr.type = mktype(u->loc, Tyvoid);
//		append(s, u);
//		t = addr(s, dst, exprtype(dst));
//		ty = exprtype(n);
//		/* we only need to clear if we don't have things fully initialized */
//		if (tybase(ty)->nmemb != n->expr.nargs)
//			append(s, mkexpr(n->loc, Oclear, t, mkintlit(n->loc, size(n)), NULL));
//		for (i = 0; i < n->expr.nargs; i++)
//			assignat(s, t, offset(n, n->expr.args[i]->expr.idx), rval(s, n->expr.args[i], NULL));
//		r = dst;
//		break;
//	case Ocast:
//		r = flattencast(s, args[0], exprtype(n));
//		break;
//
//		/* fused ops:
//		 * foo ?= blah
//		 *    =>
//		 *     foo = foo ? blah*/
//	case Oaddeq: case Osubeq: case Omuleq: case Odiveq: case Omodeq:
//	case Oboreq: case Obandeq: case Obxoreq: case Obsleq: case Obsreq:
//		assert(fusedmap[exprop(n)] != Obad);
//		u = lval(s, args[0]);
//		v = rval(s, args[1], NULL);
//		v = mkexpr(n->loc, fusedmap[exprop(n)], u, v, NULL);
//		v->expr.type = u->expr.type;
//		r = set(u, v);
//		break;
//
//		/* ++expr(x)
//		 *  => args[0] = args[0] + 1
//		 *     expr(x) */
//	case Opreinc:
//		v = assign(s, args[0], addk(args[0], 1));
//		append(s, v);
//		r = rval(s, args[0], NULL);
//		break;
//	case Opredec:
//		v = assign(s, args[0], subk(args[0], 1));
//		append(s, v);
//		r = rval(s, args[0], NULL);
//		break;
//
//		/* expr(x++)
//		 *   => expr
//		 *      x = x + 1
//		 */
	case Opostinc:
		r = lval(s, args[0]);
		t = asn(r, addk(r, 1));
		lappend(&s->incqueue, &s->nqueue, t);
		break;
	case Opostdec:
		r = lval(s, args[0]);
		t = asn(r, subk(r, 1));
		lappend(&s->incqueue, &s->nqueue, t);
		break;
	case Olit:
		switch (args[0]->lit.littype) {
		case Lvoid:
		case Lchr:
		case Lbool:
		case Llbl:
		case Lint: 
		case Lstr:
		case Lflt:
		case Lfunc:
			r = n; //capture(s, n, dst);
			break;
		}
		break;
	case Ovar:
		r = n;
		break;;
//	case Ogap:
//		fatal(n, "'_' may not be an rvalue");
//		break;
	case Oret:
		/* drain the increment queue before we return */
		t = rval(s, args[0]);
		for (i = 0; i < s->nqueue; i++)
			append(s, s->incqueue[i]);
		lfree(&s->incqueue, &s->nqueue);
		append(s, mkexpr(n->loc, Oret, t, NULL));
		break;
	case Oasn:
		if (exprop(args[0]) == Otup) {
			r = destructure(s, args[0], args[1]);
		} else if (tybase(exprtype(n))->type != Tyvoid) {
			t = lval(s, args[0]);
			u = rval(s, args[1]);
			r = asn(t, u);
		} else {
			r = rval(s, args[1]);
		}
		break;
//	case Ocall:
//		r = flattencall(s, n, dst);
//		break;
//	case Oaddr:
//		t = lval(s, args[0]);
//		if (exprop(t) == Ovar) /* Ovar is the only one that doesn't return Oderef(Oaddr(...)) */
//			r = addr(s, t, exprtype(t));
//		else
//			r = t->expr.args[0];
//		break;
//	case Oneg:
//		if (istyfloat(exprtype(n))) {
//			t =mkfloat(n->loc, -1.0); 
//			u = mkexpr(n->loc, Olit, t, NULL);
//			t->lit.type = n->expr.type;
//			u->expr.type = n->expr.type;
//			v = flattenblob(s, u);
//			r = mkexpr(n->loc, Ofmul, v, rval(s, args[0], NULL), NULL);
//			r->expr.type = n->expr.type;
//		} else {
//			r = visit(s, n);
//		}
//		break;
	case Obreak:
		r = NULL;
		if (s->nloopexit == 0)
			fatal(n, "trying to break when not in loop");
		jmp(s, s->loopexit[s->nloopexit - 1]);
		break;
	case Ocontinue:
		r = NULL;
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
//	case Otupget:
//		assert(exprop(args[1]) == Olit);
//		i = args[1]->expr.args[0]->lit.intval;
//		t = rval(s, args[0], NULL);
//		r = tupget(s, t, i, dst);
//		break;
//	case Obad:
//		die("bad operator");
//		break;
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
	if (r && n->expr.idx)
		r->expr.idx = n->expr.idx;
	return r;
}

static Node *lval(Flattenctx *s, Node *n)
{
	Node *r;

	switch (exprop(n)) {
	case Ovar:	r = n;	break;
	case Oidx:	r = rval(s, n);	break;//loadidx(s, args[0], args[1]);	break;
	case Oderef:	r = rval(s, n);	break;
	case Omemb:	r = rval(s, n);	break;
	case Ostruct:	r = rval(s, n);	break;
	case Oucon:	r = rval(s, n);	break;
	case Oarr:	r = rval(s, n);	break;
	case Ogap:	r = temp(s, n);	break;

			/* not actually expressible as lvalues in syntax, but we generate them */
	case Oudata:	r = rval(s, n);	break;
	case Outag:	r = rval(s, n);	break;
	case Otupget:	r = rval(s, n);	break;
	default:
			fatal(n, "%s cannot be an lvalue", opstr[exprop(n)]);
			break;
	}
	return r;
}

static void flattenblk(Flattenctx *fc, Node *n)
{
	size_t i;

	for (i = 0; i < n->block.nstmts; i++) {
		n->block.stmts[i] = fold(n->block.stmts[i], 0);
		flatten(fc, n->block.stmts[i]);
	}
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
static void flattenloop(Flattenctx *s, Node *n)
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

	flatten(s, n->loopstmt.init);  /* init */
	jmp(s, lcond);              /* goto test */
	flatten(s, lbody);             /* body lbl */
	flatten(s, n->loopstmt.body);  /* body */
	flatten(s, lstep);             /* test lbl */
	flatten(s, n->loopstmt.step);  /* step */
	flatten(s, lcond);             /* test lbl */
	flattencond(s, n->loopstmt.cond, lbody, lend);    /* repeat? */
	flatten(s, lend);              /* exit */

	s->nloopstep--;
	s->nloopexit--;
}

/* if foo; bar; else baz;;
 *      => cjmp (foo) :bar :baz */
static void flattenif(Flattenctx *s, Node *n, Node *exit)
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

	flattencond(s, n->ifstmt.cond, l1, l2);
	flatten(s, l1);
	flatten(s, iftrue);
	jmp(s, l3);
	flatten(s, l2);
	/* because lots of bunched up end labels are ugly,
	 * coalesce them by handling 'elif'-like constructs
	 * separately */
	if (iffalse && iffalse->type == Nifstmt) {
		flattenif(s, iffalse, exit);
	} else {
		flatten(s, iffalse);
		jmp(s, l3);
	}

	if (!exit)
		flatten(s, l3);
}

static void flattenloopmatch(Flattenctx *s, Node *pat, Node *val, Node *ltrue, Node *lfalse)
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
		flatten(s, out[i]);
	flatten(s, lload);
	for (i = 0; i < ncap; i++)
		flatten(s, cap[i]);
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
static void flattenidxiter(Flattenctx *s, Node *n)
{
	Node *lbody, *lstep, *lcond, *lmatch, *lend;
	Node *idx, *len, *dcl, *seq, *val, *done;
	Node *zero;
        Type *idxtype;

	lbody = genlbl(n->loc);
	lstep = genlbl(n->loc);
	lcond = genlbl(n->loc);
	lmatch = genlbl(n->loc);
	lend = genlbl(n->loc);

	lappend(&s->loopstep, &s->nloopstep, lstep);
	lappend(&s->loopexit, &s->nloopexit, lend);

        /* FIXME: pass this in from main() */
        idxtype = mktype(n->loc, Tyuint64);
	zero = mkintlit(n->loc, 0);
	zero->expr.type = idxtype;

	seq = rval(s, n->iterstmt.seq);
	idx = gentemp(n->loc, idxtype, &dcl);

	/* setup */
	append(s, asn(idx, zero));
	jmp(s, lcond);
	flatten(s, lbody);

	/* body */
	flatten(s, n->iterstmt.body);
	/* step */
	flatten(s, lstep);
	flatten(s, asn(idx, addk(idx, 1)));
	/* condition */
	flatten(s, lcond);
	len = seqlen(s, seq, idxtype);
	done = mkexpr(n->loc, Olt, idx, len, NULL);
	done->expr.type = mktype(n->loc, Tybool);
	cjmp(s, done, lmatch, lend);
	flatten(s, lmatch);
	val = mkexpr(n->loc, Oidx, seq, idx);
	val->expr.type = tybase(exprtype(seq))->sub[0];

	/* pattern match */
	flattenloopmatch(s, n->iterstmt.elt, val, lbody, lstep);
	jmp(s, lbody);
	flatten(s, lend);

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
			var->expr.type = dcl->decl.type;
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
static void flattentraititer(Flattenctx *s, Node *n)
{
	Node *lbody, *lclean, *lstep, *lmatch, *lend;
	Node *done, *val, *iter, *valptr, *iterptr;
	Node *func, *call;
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

	append(s, asn(iter, n->iterstmt.seq));
	jmp(s, lstep);
	flatten(s, lbody);
	/* body */
	flatten(s, n->iterstmt.body);
	flatten(s, lclean);

	/* call iterator cleanup */
	func = itertraitfn(n->loc, tr, "__iterfin__", exprtype(iter));
	call = mkexpr(n->loc, Ocall, func, iterptr, valptr, NULL);
	call->expr.type = mktype(n->loc, Tyvoid);
	append(s, call);

	flatten(s, lstep);
	/* call iterator step */
	func = itertraitfn(n->loc, tr, "__iternext__", exprtype(iter));
	call = mkexpr(n->loc, Ocall, func, iterptr, valptr, NULL);
	done = gentemp(n->loc, mktype(n->loc, Tybool), NULL);
	call->expr.type = exprtype(done);
	append(s, asn(done, call));
	cjmp(s, done, lmatch, lend);

	/* pattern match */
	flatten(s, lmatch);
	flattenloopmatch(s, n->iterstmt.elt, val, lbody, lclean);
	jmp(s, lbody);
	flatten(s, lend);

	s->nloopstep--;
	s->nloopexit--;
}

static void flatteniter(Flattenctx *s, Node *n)
{
	switch (tybase(exprtype(n->iterstmt.seq))->type) {
	case Tyarray:	flattenidxiter(s, n);	break;
	case Tyslice:	flattenidxiter(s, n);	break;
	default:	flattentraititer(s, n);	break;
	}
}
static void flattenmatch(Flattenctx *fc, Node *n)
{
	Node *val;
	Node **match;
	size_t i, nmatch;

	val = rval(fc, n->matchstmt.val);

	match = NULL;
	nmatch = 0;
	genmatch(n, val, &match, &nmatch);
	for (i = 0; i < nmatch; i++)
		flatten(fc, match[i]);
}

static void flattenexpr(Flattenctx *fc, Node *n)
{
	Node *r;
	size_t i;

	if (islbl(n)) {
		append(fc, n);
		return;
	}

	r = rval(fc, n);
	if (r)
		append(fc, r);
	for (i = 0; i < fc->nqueue; i++)
		append(fc, fc->incqueue[i]);
	lfree(&fc->incqueue, &fc->nqueue);
}

static Node *flatten(Flattenctx *fc, Node *n)
{
	Node *r, *u, *t;

	if (!n)
		return NULL;
	r = NULL;
	switch (n->type) {
	case Nblock:	flattenblk(fc, n);	break;
	case Nloopstmt:	flattenloop(fc, n);	break;
	case Niterstmt:	flatteniter(fc, n);	break;
	case Nifstmt:	flattenif(fc, n, NULL);	break;
	case Nmatchstmt:	flattenmatch(fc, n);	break;
	case Nexpr:	flattenexpr(fc, n);     break;
	case Ndecl:
		append(fc, n);
		r = mkexpr(n->loc, Ovar, n->decl.name, NULL);
		if (n->decl.init) {
			t = rval(fc, n->decl.init);
			u = mkexpr(n->loc, Oasn, r, t, NULL);
			u->expr.type = n->decl.type;
			r->expr.type = n->decl.type;
			r->expr.did = n->decl.did;
			flatten(fc, u);
		}
		break;
	default:
		dump(n, stderr);
		die("bad node passsed to flatten()");
		break;
	}
	return r;
}

static Node *flatteninit(Node *dcl)
{
	Flattenctx fc = {0,};
	Node *lit, *fn, *blk, *body;

	lit = dcl->decl.init->expr.args[0];
	fn = lit->lit.fnval;
	body = fn->func.body;
	flatten(&fc, fn->func.body);
	blk = mkblock(fn->loc, body->block.scope);
	blk->block.stmts = fc.stmts;
	blk->block.nstmts = fc.nstmts;
	fn->func.body = blk;

	return dcl;
}

static int ismain(Node *n)
{
	n = n->decl.name;
	if (n->name.ns)
		return 0;
	return strcmp(n->name.name, "main") == 0;
}

Node *flattenfn(Node *dcl)
{
	if (ismain(dcl))
		dcl->decl.vis = Vishidden;
	if (dcl->decl.isextern || dcl->decl.isgeneric)
		return dcl;
	if (isconstfn(dcl)) {
		dcl = flatteninit(dcl);
		//lappend(fn, nfn, f);
	}

	//lappend(fn, nfn, f);
	return dcl;
}

int isconstfn(Node *n)
{
	Node *d, *e;
	Type *t;

	if (n->type == Nexpr) {
		if (exprop(n) != Ovar)
			return 0;
		d = decls[n->expr.did];
	} else {
		d = n;
	}
	t = tybase(decltype(d));
	if (!d || !d->decl.isconst || !d->decl.isglobl || d->decl.isgeneric)
		return 0;
	if (t->type != Tyfunc && t->type != Tycode)
		return 0;
	e = d->decl.init;
	if (e && (exprop(e) != Olit || e->expr.args[0]->lit.littype != Lfunc))
		return 0;
	if (!e && !d->decl.isextern && !d->decl.isimport)
		return 0;
	return 1;
}

