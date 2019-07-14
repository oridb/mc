#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
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

/* forward decls */
Loc *selexpr(Isel *s, Node *n);

#define Nfloatregargs 8
#define Nintregargs 6
regid floatargregs[] = {
	Rxmm0d, Rxmm1d, Rxmm2d, Rxmm3d,
	Rxmm4d, Rxmm5d, Rxmm6d, Rxmm7d,
};
regid intargregs[] = {Rrdi, Rrsi, Rrdx, Rrcx, Rr8, Rr9};

/* used to decide which operator is appropriate
 * for implementing various conditional operators */
struct {
	AsmOp test;
	AsmOp jmp;
	AsmOp getflag;
} reloptab[Numops] = {
	[Olnot] = {Itest, Ijz, Isetz}, /* lnot invalid for floats */
	/* signed int */
	[Oeq] = {Icmp, Ijz,  Isetz},
	[One] = {Icmp, Ijnz, Isetnz},
	[Ogt] = {Icmp, Ijg,  Isetg},
	[Oge] = {Icmp, Ijge, Isetge},
	[Olt] = {Icmp, Ijl,  Isetl},
	[Ole] = {Icmp, Ijle, Isetle},
	/* unsigned int */
	[Oueq] = {Icmp, Ijz,  Isetz},
	[Oune] = {Icmp, Ijnz, Isetnz},
	[Ougt] = {Icmp, Ija,  Iseta},
	[Ouge] = {Icmp, Ijae, Isetae},
	[Oult] = {Icmp, Ijb,  Isetb},
	[Oule] = {Icmp, Ijbe, Isetbe},
	/* float */
	[Ofeq] = {Icomis, Ijz,  Isetz},
	[Ofne] = {Icomis, Ijnz, Isetnz},
	[Ofgt] = {Icomis, Ija,  Iseta},
	[Ofge] = {Icomis, Ijae, Isetae},
	[Oflt] = {Icomis, Ijb,  Isetb},
	[Ofle] = {Icomis, Ijbe, Isetbe},
};

static Mode
tymode(Type *t)
{
	/* FIXME: What should the mode for, say, structs be when we have no
	 * intention of loading /through/ the pointer? For now, we'll just say it's
	 * the pointer mode, since we expect to address through the pointer */
	t = tybase(t);
	switch (t->type) {
	case Tyflt32:	return ModeF;	break;
	case Tyflt64:	return ModeD;	break;
	default:
		if (isstacktype(t))
			return ModeQ;
		switch (tysize(t)) {
		case 1:	return ModeB;	break;
		case 2:	return ModeW;	break;
		case 4:	return ModeL;	break;
		case 8:	return ModeQ;	break;
		}
		break;
	}
	return ModeNone;
}

static Mode
mode(Node *n)
{
	if (n->type == Nexpr)
		return tymode(exprtype(n));
	else if (n->type == Ndecl)
		return tymode(n->decl.type);
	else
		die("invalid node type");
	return ModeNone;
}

static Loc *
varloc(Isel *s, Node *n)
{
	ssize_t off;
	Loc *l, *rip;

	/* we need to try getting it from the stack first, in case we
	 * forced it to stack for addressing */
	if (hthas(s->globls, n)) {
		rip = locphysreg(Rrip);
		l = locmeml(htget(s->globls, n), rip, NULL, mode(n));
	} else if (hthas(s->envoff, n)) {
		off = ptoi(htget(s->envoff, n));
		l = locmem(off, s->envp, NULL, mode(n));
	} else if (hthas(s->stkoff, n)) {
		off = ptoi(htget(s->stkoff, n));
		l = locmem(-off, locphysreg(Rrbp), NULL, mode(n));
	} else if (stacknode(n)) {
		assert(0);
	} else {
		l = htget(s->reglocs, n);
		if (!l) {
			l = locreg(mode(n));
			htput(s->reglocs, n, l);
		}
	}
	return l;
}

static Loc *
loc(Isel *s, Node *n)
{
	Node *v;
	Ucon *uc;
	Loc *l;

	if (n->type == Ndecl) {
		l = varloc(s, n);
	} else {
		switch (exprop(n)) {
		case Ovar:
			l = varloc(s, n);
			break;
		case Oucon:
			uc = finducon(exprtype(n), n->expr.args[0]);
			l = loclit(uc->id, mode(n));
			break;
		case Olit:
			v = n->expr.args[0];
			switch (v->lit.littype) {
			case Lvoid:	l = NULL;	break;
			case Lchr:	l = loclit(v->lit.chrval, mode(n));	break;
			case Lbool:	l = loclit(v->lit.boolval, mode(n));	break;
			case Lint:	l = loclit(v->lit.intval, mode(n));	break;
			default:
					die("Literal type %s should be blob", litstr[v->lit.littype]);
			}
			break;
		default:
			die("Node %s not leaf in loc()", opstr[exprop(n)]);
			break;
		}
	}
	return l;
}

static Insn *
mkinsnv(AsmOp op, va_list ap)
{
	Loc *l;
	Insn *i;
	int n;
	static size_t insnid;

	n = 0;
	i = malloc(sizeof(Insn));
	i->op = op;
	i->uid = insnid++;
	while ((l = va_arg(ap, Loc*)) != NULL)
		i->args[n++] = l;
	i->nargs = n;
	return i;
}

Insn *
mkinsn(int op, ...)
{
	va_list ap;
	Insn *i;

	va_start(ap, op);
	i = mkinsnv(op, ap);
	va_end(ap);
	return i;
}

static void
g(Isel *s, int op, ...)
{
	va_list ap;
	Insn *i;

	va_start(ap, op);
	i = mkinsnv(op, ap);
	va_end(ap);
	if (debugopt['i']) {
		printf("GEN[uid=%zd] ", i->uid);
		dbgiprintf(stdout, i);
	}
	lappend(&s->curbb->il, &s->curbb->ni, i);
}

static void
movz(Isel *s, Loc *src, Loc *dst)
{
	if (src->mode == dst->mode)
		g(s, Imov, src, dst, NULL);
	else
		g(s, Imovzx, src, dst, NULL);
}

static void
load(Isel *s, Loc *a, Loc *b)
{
	Loc *l;

	assert(b->type == Locreg);
	if (a->type == Locreg)
		l = locmem(0, b, NULL, a->mode);
	else
		l = a;
	if (isfloatmode(b->mode))
		g(s, Imovs, l, b, NULL);
	else
		g(s, Imov, l, b, NULL);
}

static void
stor(Isel *s, Loc *a, Loc *b)
{
	Loc *l;

	assert(a->type == Locreg || a->type == Loclit);
	if (b->type == Locreg)
		l = locmem(0, b, NULL, b->mode);
	else
		l = b;
	if (isfloatmode(b->mode))
		g(s, Imovs, a, l, NULL);
	else
		g(s, Imov, a, l, NULL);
}

/* ensures that a location is within a reg */
static Loc *
newr(Isel *s, Loc *a)
{
	Loc *r;

	r = locreg(a->mode);
	if (a->type == Locreg) {
		if (isfloatmode(a->mode))
			g(s, Imovs, a, r, NULL);
		else
			g(s, Imov, a, r, NULL);
	} else {
		load(s, a, r);
	}
	return r;
}

static Loc *
inr(Isel *s, Loc *a)
{
	if (a->type == Locreg)
		return a;
	return newr(s, a);
}

/* ensures that a location is within a reg or an imm */
static Loc *
inri(Isel *s, Loc *a)
{
	if (a->type == Locreg || a->type == Loclit)
		return a;
	else
		return newr(s, a);
}

/* If we're testing equality, etc, it's a bit silly
 * to generate the test, store it to a bite, expand it
 * to the right width, and then test it again. Try to optimize
 * for these common cases.
 *
 * if we're doing the optimization to avoid
 * multiple tests, we want to eval the children
 * of the first arg, instead of the first arg
 * directly */
static void
selcjmp(Isel *s, Node *n, Node **args)
{
	Loc *a, *b;
	Loc *l1, *l2;
	AsmOp cond, jmp;

	cond = reloptab[exprop(args[0])].test;
	jmp = reloptab[exprop(args[0])].jmp;
	/* if we have a cond, we're knocking off the redundant test,
	 * and want to eval the children */
	if (cond) {
		a = selexpr(s, args[0]->expr.args[0]);
		if (args[0]->expr.nargs == 2)
			b = selexpr(s, args[0]->expr.args[1]);
		else
			b = a;
		a = newr(s, a);
	} else {
		cond = Itest;
		jmp = Ijnz;
		b = newr(s, selexpr(s, args[0])); /* cond */
		a = b;
	}

	/* the jump targets will always be evaluated the same way */
	l1 = loclbl(args[1]); /* if true */
	l2 = loclbl(args[2]); /* if false */

	g(s, cond, b, a, NULL);
	g(s, jmp, l1, NULL);
	g(s, Ijmp, l2, NULL);
}

/* Generate variable length jump. There are 3 case
 * for this:
 *
 * 1) Short list: Simple comparison sequence.
 * 2) Sparse list: binary search
 * 3) Dense list: Jump table
 *
 * If a value is not in the jump table, then the 0th
 * value is used.
 */
static void
selvjmp(Isel *s, Node *n, Node **args)
{
}

static Loc *
binop(Isel *s, AsmOp op, Node *x, Node *y)
{
	Loc *a, *b;

	a = selexpr(s, x);
	b = selexpr(s, y);
	a = newr(s, a);
	g(s, op, b, a, NULL);
	return a;
}

/* We have a few common cases to optimize here:
 *    Oaddr(expr)
 * or:
 *    Oadd(
 *        reg,
 *        reg||const)
 *
 * or:
 *    Oadd(
 *        reg,
 *        Omul(reg,
 *             2 || 4 || 8)))
 */
static int
ismergablemul(Node *n, int *r)
{
	int v;

	if (exprop(n) != Omul)
		return 0;
	if (exprop(n->expr.args[1]) != Olit)
		return 0;
	if (n->expr.args[1]->expr.args[0]->type != Nlit)
		return 0;
	if (n->expr.args[1]->expr.args[0]->lit.littype != Lint)
		return 0;
	v = n->expr.args[1]->expr.args[0]->lit.intval;
	if (v != 2 && v != 4 && v != 8)
		return 0;
	*r = v;
	return 1;
}

static Loc *
memloc(Isel *s, Node *e, Mode m)
{
	Node **args;
	Loc *l, *b, *o; /* location, base, offset */
	int scale;

	scale = 1;
	l = NULL;
	args = e->expr.args;
	if (exprop(e) == Oadd) {
		b = selexpr(s, args[0]);
		if (ismergablemul(args[1], &scale))
			o = selexpr(s, args[1]->expr.args[0]);
		else
			o = selexpr(s, args[1]);

		if (b->type != Locreg)
			b = inr(s, b);
		if (o->type == Loclit) {
			l = locmem(scale*o->lit, b, NULL, m);
		} else {
			b = inr(s, b);
			o = inr(s, o);
			l = locmems(0, b, o, scale, m);
		}
	} else {
		l = selexpr(s, e);
		l = inr(s, l);
		l = locmem(0, l, NULL, m);
	}
	assert(l != NULL);
	return l;
}

static const
Mode szmodes[] = {
	[8] = ModeQ,
	[4] = ModeL,
	[2] = ModeW,
	[1] = ModeB
};
static void
blit(Isel *s, Loc *to, Loc *from, size_t dstoff, size_t srcoff, size_t sz, size_t align)
{
	size_t i, modesz;
	Loc *sp, *dp; /* pointers to src, dst */
	Loc *tmp, *src, *dst; /* source memory, dst memory */

	assert(szmodes[align] != ModeNone);   /* make sure we have a valid alignment */
	if (!sz)
		return;
	sp = inr(s, from);
	dp = inr(s, to);

	i = 0;
	if (align == 0)
		align = 8;
	for (modesz = align; szmodes[modesz] != ModeNone; modesz /= 2) {
		tmp = locreg(szmodes[modesz]);
		while (i + modesz <= sz) {
			src = locmem(i + srcoff, sp, NULL, szmodes[modesz]);
			dst = locmem(i + dstoff, dp, NULL, szmodes[modesz]);
			g(s, Imov, src, tmp, NULL);
			g(s, Imov, tmp, dst, NULL);
			i += modesz;
		}
	}

}

static void
clear(Isel *s, Loc *val, size_t sz, size_t align)
{
	Loc *dp, *zero, *dst; /* source memory, dst memory */
	size_t modesz, i;

	i = 0;
	dp = inr(s, val);
	if (align == 0)
		align = 8;
	for (modesz = align; szmodes[modesz] != ModeNone; modesz /= 2) {
		zero = locreg(szmodes[modesz]);
		g(s, Ixor, zero, zero, NULL);
		while (i + modesz <= sz) {
			dst = locmem(i, dp, NULL, szmodes[modesz]);
			g(s, Imov, zero, dst, NULL);
			i += modesz;
		}
	}
}

static void
call(Isel *s, Node *n)
{
	AsmOp op;
	Node *fn;
	Loc *f, *e;

	if (exprop(n) == Ocall) {
		op = Icall;
		fn = n->expr.args[0];
		assert(tybase(exprtype(fn))->type == Tycode);
		f = locmeml(htget(s->globls, fn), NULL, NULL, mode(fn));
	} else {
		op = Icallind;
		f = selexpr(s, n->expr.args[0]);
		e = selexpr(s, n->expr.args[1]);
		g(s, Imov, e, locphysreg(Rrax), NULL);
	}
	g(s, op, f, NULL);
}

static size_t
countargs(Type *t)
{
	size_t nargs;

	t = tybase(t);
	nargs = t->nsub - 1;
	if (isstacktype(t->sub[0]))
		nargs++;
	/* valists are replaced with hidden type parameter,
	 * which we want on the stack for ease of ABI */
	if (tybase(t->sub[t->nsub - 1])->type == Tyvalist)
		nargs--;
	return nargs;
}

static Loc *
gencall(Isel *s, Node *n)
{
	Loc *src, *dst, *arg;	/* values we reduced */
	size_t argsz, argoff, nargs, vasplit;
	size_t nfloats, nints;
	Loc *retloc, *rsp, *ret;	/* hard-coded registers */
	Loc *stkbump;	/* calculated stack offset */
	Type *t, *fn;
	Node **args;
	size_t i, a;
	int vararg;

	rsp = locphysreg(Rrsp);
	t = exprtype(n);
	if (tybase(t)->type == Tyvoid || isstacktype(t)) {
		retloc = NULL;
		ret = NULL;
	} else if (istyfloat(t)) {
		retloc = coreg(Rxmm0d, mode(n));
		ret = locreg(mode(n));
	} else {
		retloc = coreg(Rrax, mode(n));
		ret = locreg(mode(n));
	}
	fn = tybase(exprtype(n->expr.args[0]));
	/* calculate the number of args we expect to see, adjust
	 * for a hidden return argument. */
	vasplit = countargs(fn);
	argsz = 0;
	if (exprop(n) == Ocall) {
		args = &n->expr.args[1];
		nargs = n->expr.nargs - 1;
	} else {
		args = &n->expr.args[2];
		nargs = n->expr.nargs - 2;
	}
	/* Have to calculate the amount to bump the stack
	 * pointer by in one pass first, otherwise if we push
	 * one at a time, we evaluate the args in reverse order.
	 * Not good.
	 *
	 * Skip the first operand, since it's the function itself */
	for (i = 0; i < nargs; i++) {
		argsz = align(argsz, min(size(args[i]), Ptrsz));
		argsz += size(args[i]);
	}
	argsz = align(argsz, 16);
	stkbump = loclit(argsz, ModeQ);
	if (argsz)
		g(s, Isub, stkbump, rsp, NULL);

	/* Now, we can evaluate the arguments */
	argoff = 0;
	nfloats = 0;
	nints = 0;
	vararg = 0;
	for (i = 0; i < nargs; i++) {
		arg = selexpr(s, args[i]);
		argoff = alignto(argoff, exprtype(args[i]));
		if (i >= vasplit)
			vararg = 1;
		if (stacknode(args[i])) {
			src = locreg(ModeQ);
			g(s, Ilea, arg, src, NULL);
			a = tyalign(exprtype(args[i]));
			blit(s, rsp, src, argoff, 0, size(args[i]), a);
			argoff += size(args[i]);
		} else if (!vararg && isfloatmode(arg->mode) && nfloats < Nfloatregargs) {
			dst = coreg(floatargregs[nfloats], arg->mode);
			arg = inri(s, arg);
			g(s, Imovs, arg, dst, NULL);
			nfloats++;
		} else if (!vararg && isintmode(arg->mode) && nints < Nintregargs) {
			dst = coreg(intargregs[nints], arg->mode);
			arg = inri(s, arg);
			g(s, Imov, arg, dst, NULL);
			nints++;
		} else {
			dst = locmem(argoff, rsp, NULL, arg->mode);
			arg = inri(s, arg);
			stor(s, arg, dst);
			argoff += size(args[i]);
		}
	}
	call(s, n);
	if (argsz)
		g(s, Iadd, stkbump, rsp, NULL);
	if (retloc) {
		if (isfloatmode(retloc->mode))
			g(s, Imovs, retloc, ret, NULL);
		else
			g(s, Imov, retloc, ret, NULL);
	}
	return ret;
}

static Loc*
rolop(Isel *s, Node *n, Node *l, Node *r)
{
	int64_t lv, rv;
	Type *ty;

	ty = tybase(exprtype(n));

	if (!istyunsigned(ty))
		return NULL;
	if (exprop(l) != Obsl && exprop(l) != Obsr)
		return NULL;
	if (exprop(r) != Obsl && exprop(r) != Obsr)
		return NULL;
	if (exprop(r) == exprop(l))
		return NULL;
	if (exprop(l->expr.args[0]) != Ovar)
		return NULL;
	if (exprop(r->expr.args[0]) != Ovar)
		return NULL;
	if (l->expr.args[0]->expr.did != r->expr.args[0]->expr.did)
		return NULL;

	if (exprop(l->expr.args[1]) != Olit)
		return NULL;
	if (l->expr.args[1]->expr.args[0]->type != Nlit)
		return NULL;
	if (l->expr.args[1]->expr.args[0]->lit.littype != Lint)
		return NULL;
	lv = l->expr.args[1]->expr.args[0]->lit.intval;

	if (exprop(r->expr.args[1]) != Olit)
		return NULL;
	if (r->expr.args[1]->expr.args[0]->type != Nlit)
		return NULL;
	if (r->expr.args[1]->expr.args[0]->lit.littype != Lint)
		return NULL;
	rv = r->expr.args[1]->expr.args[0]->lit.intval;

	if (lv + rv != 8*size(n))
		return NULL;

	if (exprop(l) == Obsl)
		return binop(s, Irol, l->expr.args[0], l->expr.args[1]);
	else
		return binop(s, Irol, r->expr.args[0], r->expr.args[1]);
}

Loc *
selexpr(Isel *s, Node *n)
{
	Loc *a, *b, *c, *d, *r;
	Loc *edx, *cl; /* x86 wants some hard-coded regs */
	Node **args;
	size_t al;
	Op op;

	args = n->expr.args;
	edx = locphysreg(Redx);
	cl = locphysreg(Rcl);
	r = NULL;
	switch (exprop(n)) {
	case Oadd:	r = binop(s, Iadd, args[0], args[1]);	break;
	case Osub:	r = binop(s, Isub, args[0], args[1]);	break;
	case Oband:	r = binop(s, Iand, args[0], args[1]);	break;
	case Obxor:	r = binop(s, Ixor, args[0], args[1]);	break;
	case Obor:
		r = rolop(s, n, args[0], args[1]);
		if (!r)
			r = binop(s, Ior,  args[0], args[1]);
		break;
	case Omul:
		if (size(args[0]) == 1) {
			a = selexpr(s, args[0]);
			b = inr(s, selexpr(s, args[1]));

			c = locphysreg(Ral);
			r = locreg(a->mode);
			g(s, Imov, a, c, NULL);
			g(s, Iimul_r, b, NULL);
			g(s, Imov, c, r, NULL);
		} else {
			r = binop(s, Iimul, args[0], args[1]);
		}
		break;
	case Odiv:
	case Omod:
		/* these get clobbered by the div insn */
		a = selexpr(s, args[0]);
		b = selexpr(s, args[1]);
		b = newr(s, b);
		c = coreg(Reax, mode(n));
		r = locreg(a->mode);
		g(s, Imov, a, c, NULL);
		if (istysigned(exprtype(args[0]))) {
			switch (r->mode) {
			case ModeB:	g(s, Imovsx, c, coreg(Rrax, ModeW), NULL);	break;
			case ModeW:	g(s, Icwd, NULL);	break;
			case ModeL:	g(s, Icdq, NULL);	break;
			case ModeQ:	g(s, Icqo, NULL);	break;
			default:	die("invalid mode in division"); break;
			}
			g(s, Iidiv, b, NULL);
		} else {
			if (r->mode == ModeB)
				g(s, Ixor, locphysreg(Rah), locphysreg(Rah), NULL);
			else
				g(s, Ixor, edx, edx, NULL);
			g(s, Idiv, b, NULL);
		}
		if (exprop(n) == Odiv)
			d = coreg(Reax, mode(n));
		else if (r->mode != ModeB)
			d = coreg(Redx, mode(n));
		else
			d = locphysreg(Rah);
		g(s, Imov, d, r, NULL);
		break;
	case Oneg:
		r = selexpr(s, args[0]);
		r = newr(s, r);
		g(s, Ineg, r, NULL);
		break;

		/* fp expressions */
	case Ofadd:	r = binop(s, Iadds, args[0], args[1]);	break;
	case Ofsub:	r = binop(s, Isubs, args[0], args[1]);	break;
	case Ofmul:	r = binop(s, Imuls, args[0], args[1]);	break;
	case Ofdiv:	r = binop(s, Idivs, args[0], args[1]);	break;
	case Ofneg:
		r = selexpr(s, args[0]);
		r = newr(s, r);
		a = NULL;
		b = NULL;
		if (mode(args[0]) == ModeF) {
			a = locreg(ModeF);
			b = loclit(1LL << (31), ModeF);
			g(s, Imovs, r, a);
		} else if (mode(args[0]) == ModeD) {
			a = locreg(ModeQ);
			b = loclit(1LL << 63, ModeQ);
			g(s, Imov, r, a, NULL);
		}
		g(s, Ixor, b, a, NULL);
		g(s, Imov, a, r, NULL);
		break;
	case Obsl:
	case Obsr:
		a = newr(s, selexpr(s, args[0]));
		b = selexpr(s, args[1]);
		if (b->type == Loclit) {
			d = b;
		} else {
			c = coreg(Rcl, b->mode);
			g(s, Imov, b, c, NULL);
			d = cl;
		}
		if (exprop(n) == Obsr) {
			if (istysigned(n->expr.type))
				g(s, Isar, d, a, NULL);
			else
				g(s, Ishr, d, a, NULL);
		} else {
			g(s, Ishl, d, a, NULL);
		}
		r = a;
		break;
	case Obnot:
		r = selexpr(s, args[0]);
		r = newr(s, r);
		g(s, Inot, r, NULL);
		break;

	case Oderef:
		r = memloc(s, args[0], mode(n));
		break;

	case Oaddr:
		a = selexpr(s, args[0]);
		if (a->type == Loclbl || (a->type == Locmeml && !a->mem.base)) {
			r = loclitl(a->lbl);
		} else {
			r = locreg(ModeQ);
			g(s, Ilea, a, r, NULL);
		}
		break;

	case Olnot:
		a = newr(s, selexpr(s, args[0]));
		b = locreg(ModeB);
		r = locreg(mode(n));
		/* lnot only valid for integer-like values */
		g(s, reloptab[exprop(n)].test, a, a, NULL);
		g(s, reloptab[exprop(n)].getflag, b, NULL);
		movz(s, b, r);
		break;

	case Oeq: case One: case Ogt: case Oge: case Olt: case Ole:
	case Ofeq: case Ofne: case Ofgt: case Ofge: case Oflt: case Ofle:
	case Oueq: case Oune: case Ougt: case Ouge: case Oult: case Oule:
		a = selexpr(s, args[0]);
		b = selexpr(s, args[1]);
		a = newr(s, a);
		c = locreg(ModeB);
		r = locreg(mode(n));
		g(s, reloptab[exprop(n)].test, b, a, NULL);
		g(s, reloptab[exprop(n)].getflag, c, NULL);
		movz(s, c, r);
		return r;

	case Oasn:  /* relabel */
		die("Unimplemented op %s", opstr[exprop(n)]);
		break;
	case Oset:
		op = exprop(args[0]);
		assert(op == Ovar || op == Oderef || op == Ogap);
		assert(!stacknode(args[0]));

		if (op == Ogap)
			break;

		b = selexpr(s, args[1]);
		if (exprop(args[0]) == Oderef)
			a = memloc(s, args[0]->expr.args[0], mode(n));
		else
			a = selexpr(s, args[0]);
		b = inri(s, b);
		if (isfloatmode(b->mode))
			g(s, Imovs, b, a, NULL);
		else
			g(s, Imov, b, a, NULL);
		r = b;
		break;
	case Ocall:
	case Ocallind:
		r = gencall(s, n);
		break;
	case Oret:
		a = locstrlbl(s->cfg->end->lbls[0]);
		g(s, Ijmp, a, NULL);
		break;
	case Ojmp:
		g(s, Ijmp, loclbl(args[0]), NULL);
		break;
	case Ocjmp:
		selcjmp(s, n, args);
		break;
	case Ovjmp:
		selvjmp(s, n, args);
		break;
	case Oucon:
		assert(isenum(tybase(exprtype(n))));
		/* fallthrough */
	case Olit:
		r = loc(s, n);
		break;
	case Ovar:
		if (isconstfn(n)) {
			r = locreg(ModeQ);
			a = loc(s, n);
			g(s, Ilea, a, r, NULL);
		} else {
			r = loc(s, n);
		}
		break;
	case Ogap:
		break;
	case Oblit:
		a = selexpr(s, args[0]);
		r = selexpr(s, args[1]);
		al = alignto(1, args[0]->expr.type->sub[0]);
		blit(s, a, r, 0, 0, args[2]->expr.args[0]->lit.intval, al);
		break;

	case Oclear:
		a = selexpr(s, args[0]);
		clear(s, a, args[1]->expr.args[0]->lit.intval, 0);
		break;

		/* cast operators that actually modify the values */
	case Otrunc:
		a = selexpr(s, args[0]);
		a = inr(s, a);
		r = locreg(mode(n));
		g(s, Imov, a, r, NULL);
		break;
	case Ozwiden:
		a = selexpr(s, args[0]);
		a = inr(s, a);
		r = locreg(mode(n));
		movz(s, a, r);
		break;
	case Oswiden:
		a = selexpr(s, args[0]);
		a = inr(s, a);
		r = locreg(mode(n));
		g(s, Imovsx, a, r, NULL);
		break;
	case Oint2flt:
		a = selexpr(s, args[0]);
		a = inr(s ,a);
		r = locreg(mode(n));
		g(s, Icvttsi2sd, a, r, NULL);
		break;
	case Oflt2int:
		a = selexpr(s, args[0]);
		a = inr(s ,a);
		r = locreg(mode(n));
		g(s, Icvttsd2si, a, r, NULL);
		break;

	case Oflt2flt:
		a = selexpr(s, args[0]);
		r = locreg(mode(n));
		if (a->mode == ModeD)
			g(s, Icvttsd2ss, a, r, NULL);
		else
			g(s, Icvttss2sd, a, r, NULL);
		break;
	case Odead:
	case Oundef:
	case Odef:
		/* nothing */
		break;

		/* These operators should never show up in the reduced trees,
		 * since they should have been replaced with more primitive
		 * expressions by now */
	case Obad: case Oauto: case Opreinc: case Opostinc: case Opredec:
	case Otern: case Opostdec: case Olor: case Oland: case Oaddeq:
	case Osubeq: case Omuleq: case Odiveq: case Omodeq: case Oboreq:
	case Obandeq: case Obxoreq: case Obsleq: case Obsreq: case Omemb:
	case Oslbase: case Osllen: case Ocast: case Outag: case Oudata:
	case Otup: case Oarr: case Ostruct:
	case Oslice: case Oidx: case Osize: case Otupget: case Otupmemb:
	case Obreak: case Ocontinue:
	case Numops:
		dumpn(n, stdout);
		die("Should not see %s in isel", opstr[exprop(n)]);
		break;
	}
	return r;
}

static void
isel(Isel *s, Node *n)
{
	switch (n->type) {
	case Nexpr:
		selexpr(s, n);
		break;
	case Ndecl:
		break;
	default:
		die("Bad node type in isel()");
		break;
	}
}

/* %rax is for int returns, %xmm0d is for floating returns */
Reg
savedregs[] = {
	Rr12, Rr13, Rr14, Rr15,
	Rnone
};

void
addarglocs(Isel *s, Func *fn)
{
	size_t i, nints, nfloats, nargs;
	size_t argoff;
	int vararg;
	Node *arg;
	Loc *a, *l;

	argoff = 0;
	nfloats = 0;
	nints = 0;
	vararg = 0;
	nargs = countargs(fn->type);
	for (i = 0; i < fn->nargs; i++) {
		arg = fn->args[i];
		argoff = alignto(argoff, decltype(arg));
		if (i >= nargs)
			vararg = 1;
		if (stacknode(arg)) {
			htput(s->stkoff, arg, itop(-(argoff + 2*Ptrsz)));
			argoff += size(arg);
		} else if (!vararg && isfloatmode(mode(arg)) && nfloats < Nfloatregargs) {
			a = coreg(floatargregs[nfloats], mode(arg));
			l = loc(s, arg);
			g(s, Imovs, a, l, NULL);
			htput(s->reglocs, arg, l);
			nfloats++;
		} else if (!vararg && isintmode(mode(arg)) && nints < Nintregargs) {
			a = coreg(intargregs[nints], mode(arg));
			l = loc(s, arg);
			g(s, Imov, a, l, NULL);
			htput(s->reglocs, arg, l);
			nints++;
		} else if (tybase(decltype(arg))->type != Tyvoid) {
			/* varargs go on the stack */
			htput(s->stkoff, arg, itop(-(argoff + 2*Ptrsz)));
			argoff += size(arg);
		}
	}
}

static void
prologue(Isel *s, Func *fn, size_t sz)
{
	Loc *rsp;
	Loc *rbp;
	Loc *stksz;
	Loc *phys;
	size_t i;

	rsp = locphysreg(Rrsp);
	rbp = locphysreg(Rrbp);
	stksz = loclit(sz, ModeQ);
	/* enter function */
	g(s, Ipush, rbp, NULL);
	g(s, Imov, rsp, rbp, NULL);
	g(s, Isub, stksz, rsp, NULL);
	/* save registers */
	for (i = 0; savedregs[i] != Rnone; i++) {
		phys = locphysreg(savedregs[i]);
		s->calleesave[i] = locreg(phys->mode);
		if (isfloatmode(phys->mode)) {
			g(s, Imovs, phys, s->calleesave[i], NULL);
		} else {
			g(s, Imov, phys, s->calleesave[i], NULL);
		}
	}
	if (s->envp)
		g(s, Imov, locphysreg(Rrax), s->envp, NULL);
	addarglocs(s, fn);
	s->nsaved = i;
	s->stksz = stksz; /* need to update if we spill */
}

static void
epilogue(Isel *s)
{
	Loc *rsp, *rbp;
	Loc *ret;
	size_t i;

	rsp = locphysreg(Rrsp);
	rbp = locphysreg(Rrbp);
	if (s->ret) {
		ret = loc(s, s->ret);
		if (istyfloat(exprtype(s->ret)))
			g(s, Imovs, ret, coreg(Rxmm0d, ret->mode), NULL);
		else
			g(s, Imov, ret, coreg(Rax, ret->mode), NULL);
	}
	/* restore registers */
	for (i = 0; savedregs[i] != Rnone; i++) {
		if (isfloatmode(s->calleesave[i]->mode)) {
			g(s, Imovs, s->calleesave[i], locphysreg(savedregs[i]), NULL);
		} else {
			g(s, Imov, s->calleesave[i], locphysreg(savedregs[i]), NULL);
		}
	}
	/* leave function */
	g(s, Imov, rbp, rsp, NULL);
	g(s, Ipop, rbp, NULL);
	g(s, Iret, NULL);
}

static Asmbb *
mkasmbb(Bb *bb)
{
	Asmbb *as;

	if (!bb)
		return NULL;
	as = zalloc(sizeof(Asmbb));
	as->id = bb->id;
	as->pred = bsdup(bb->pred);
	as->succ = bsdup(bb->succ);
	as->lbls = memdup(bb->lbls, bb->nlbls*sizeof(char*));
	as->nlbls = bb->nlbls;
	return as;
}

void
selfunc(Isel *is, Func *fn, Htab *globls, Htab *strtab)
{
	int fileid, lastline;
	Node *n;
	Bb *bb;
	size_t i, j;
	char buf[128];
	char *path;


	for (i = 0; i < fn->cfg->nbb; i++)
		lappend(&is->bb, &is->nbb, mkasmbb(fn->cfg->bb[i]));

	is->curbb = is->bb[0];

	fileid = fn->loc.file;
	if (fileid >= 0 && fn->loc.line > 0) {
		path = file.files[fileid];
		bprintf(buf, sizeof buf, "%s/%s:%d", is->cwd, path, fn->loc.line);
		g(is, Icomment, locstrlbl(buf), NULL);
		bprintf(buf, sizeof buf, "%zd %d", fileid + 1, fn->loc.line);
		g(is, Iloc, locstrlbl(buf), NULL);
	}

	prologue(is, fn, fn->stksz);
	lastline = -1;
	for (j = 0; j < fn->cfg->nbb - 1; j++) {
		is->curbb = is->bb[j];
		if (!is->bb[j])
			continue;
		bb = fn->cfg->bb[j];
		for (i = 0; i < bb->nnl; i++) {
			/* put in a comment that says where this line comes from */
			n = bb->nl[i];
			fileid = n->loc.file;
			if (n->loc.file >= 0 && n->loc.line != -1 && n->loc.line != lastline) {
				lastline = n->loc.line;
				path = file.files[fileid];
				bprintf(buf, sizeof buf, "%s/%s:%d", is->cwd, path, n->loc.line);
				g(is, Icomment, locstrlbl(buf), NULL);
				bprintf(buf, sizeof buf, "%zd %d", fileid + 1, n->loc.line);
				g(is, Iloc, locstrlbl(buf), NULL);
			}
			isel(is, fn->cfg->bb[j]->nl[i]);
		}
	}
	is->curbb = is->bb[is->nbb - 1];
	epilogue(is);
	peep(is);
	regalloc(is);
	is->stksz->lit = align(is->stksz->lit, 16);
}
