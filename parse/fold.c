#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "parse.h"

size_t (*sizefn)(Node *n);

int
getintlit(Node *n, vlong *v)
{
	Node *l;

	if (exprop(n) != Olit)
		return 0;
	l = n->expr.args[0];
	/* It's up to type checking to make sure this is ok */
	if (l->lit.littype == Lint)
		*v = l->lit.intval;
	else if (l->lit.littype == Lchr)
		*v = l->lit.chrval;
	else
		return 0;
	return 1;
}

static int
isintval(Node *n, vlong val)
{
	vlong v;

	if (!getintlit(n, &v))
		return 0;
	return v == val;
}

static Node *
val(Srcloc loc, vlong val, Type *t)
{
	Node *l, *n;

	l = mkint(loc, val);
	n = mkexpr(loc, Olit, l, NULL);
	l->lit.type = t;
	n->expr.type = t;
	return n;
}

static int
issmallconst(Node *dcl)
{
	Type *t;

	if (!dcl->decl.isconst)
		return 0;
	if (!dcl->decl.init)
		return 0;
	t = tybase(exprtype(dcl->decl.init));
	if (t->type <= Tyflt64)
		return 1;
	return 0;
}

static Node *
foldcast(Node *n)
{
	Type *to, *from;
	Node *sub;

	sub = n->expr.args[0];
	to = exprtype(n);
	from = exprtype(sub);

	switch (tybase(to)->type) {
	case Tybool:
	case Tyint8: case Tyint16: case Tyint32: case Tyint64:
	case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
	case Tyint: case Tyuint: case Tychar: case Tybyte:
	case Typtr:
		switch (tybase(from)->type) {
		case Tybool:
		case Tyint8: case Tyint16: case Tyint32: case Tyint64:
		case Tyuint8: case Tyuint16: case Tyuint32: case Tyuint64:
		case Tyint: case Tyuint: case Tychar: case Tybyte:
		case Typtr:
			if (exprop(sub) == Olit || tybase(from)->type == tybase(to)->type) {
				sub->expr.type = to;
				return sub;
			} else {
				return n;
			}
		default:
			return n;
		}
	default:
		return n;
	}
	return n;
}

int
idxcmp(const void *pa, const void *pb)
{
	Node *a, *b;
	vlong av, bv;

	a = *(Node **)pa;
	b = *(Node **)pb;

	assert(getintlit(a->expr.idx, &av));
	assert(getintlit(b->expr.idx, &bv));

	/* don't trust overflow with int64 */
	if (av < bv)
		return -1;
	else if (av == bv)
		return 0;
	else
		return 1;
}

Node *
fold(Node *n, int foldvar)
{
	Node **args, *r;
	Type *t;
	vlong a, b;
	Ucon *uc;
	size_t i;

	if (!n)
		return NULL;
	if (n->type != Nexpr)
		return n;

	r = NULL;
	args = n->expr.args;
	if (n->expr.idx)
		n->expr.idx = fold(n->expr.idx, foldvar);
	for (i = 0; i < n->expr.nargs; i++)
		args[i] = fold(args[i], foldvar);
	switch (exprop(n)) {
	case Outag:
		if (exprop(args[0]) != Oucon)
			break;
		uc = finducon(tybase(exprtype(args[0])), args[0]->expr.args[0]);
		r = val(n->loc, uc->id, exprtype(n));
		break;
	case Oudata:
		if (exprop(args[0]) != Oucon || args[0]->expr.nargs != 2)
			break;
		r = args[0]->expr.args[1];
		break;
	case Ovar:
		if (foldvar && issmallconst(decls[n->expr.did]))
			r = fold(decls[n->expr.did]->decl.init, foldvar);
		break;
	case Oadd:
		/* x + 0 = 0 */
		if (isintval(args[0], 0))
			r = args[1];
		if (isintval(args[1], 0))
			r = args[0];
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a + b, exprtype(n));
		break;
	case Osub:
		/* x - 0 = 0 */
		if (isintval(args[1], 0))
			r = args[0];
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, (uint64_t)a - b, exprtype(n));
		break;
	case Omul:
		/* 1 * x = x */
		if (isintval(args[0], 1))
			r = args[1];
		if (isintval(args[1], 1))
			r = args[0];
		/* 0 * x = 0 */
		if (isintval(args[0], 0))
			r = args[0];
		if (isintval(args[1], 0))
			r = args[1];
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a * b, exprtype(n));
		break;
	case Odiv:
		/* x/0 = error */
		if (isintval(args[1], 0))
			fatal(args[1], "division by zero");
		/* x/1 = x */
		if (isintval(args[1], 1))
			r = args[0];
		/* 0/x = 0 */
		if (isintval(args[1], 0))
			r = args[1];
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a / b, exprtype(n));
		break;
	case Omod:
		/* x%0 = error */
		if (isintval(args[1], 0))
			fatal(args[1], "division by zero");
		/* x%1 = 0 */
		if (isintval(args[1], 1))
			r = val(n->loc, 0, exprtype(n));
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a % b, exprtype(n));
		break;
	case Oneg:
		if (getintlit(args[0], &a))
			r = val(n->loc, -a, exprtype(n));
		break;
	case Obsl:
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, (uint64_t)a << b, exprtype(n));
		break;
	case Obsr:
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a >> b, exprtype(n));
		break;
	case Obor:
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a | b, exprtype(n));
		break;
	case Oband:
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a & b, exprtype(n));
		break;
	case Obxor:
		if (getintlit(args[0], &a) && getintlit(args[1], &b))
			r = val(n->loc, a ^ b, exprtype(n));
		break;
	case Omemb:
		t = tybase(exprtype(args[0]));
		/* we only fold lengths right now */
		if (t->type == Tyarray && !strcmp(namestr(args[1]), "len")) {
			r = t->asize;
			r->expr.type = exprtype(n);
		}
		break;
	case Oarr:
                if (n->expr.nargs > 0)
                    qsort(n->expr.args, n->expr.nargs, sizeof(Node*), idxcmp);
		break;
	case Ocast:
		r = foldcast(n);
		break;
	case Osize:
		if (sizefn)
			r = val(n->loc, sizefn(n->expr.args[0]), exprtype(n));
		break;
	default:
		break;
	}

	if (r && n->expr.idx)
		r->expr.idx = n->expr.idx;

	if (r)
		return r;
	else
		return n;
}

