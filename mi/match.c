#include <stdio.h>
#include <stdlib.h>
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
#include "mi.h"
#include "dtree.h"

Dtree *gendtree(Node *m, Node *val, Node **lbl, size_t nlbl, int startid);
Dtree *gendtree2(Node *m, Node *val, Node **lbl, size_t nlbl, int startid);
static int addpat(Node *pat, Node *val,
		Dtree *start, Dtree *accept,
		Node ***cap, size_t *ncap,
		Dtree ***end, size_t *nend);
void dtreedump(FILE *fd, Dtree *dt);


static int ndtree;

static Node *
utag(Node *n)
{
	Node *tag;

	tag = mkexpr(n->loc, Outag, n, NULL);
	tag->expr.type = mktype(n->loc, Tyint32);
	return tag;
}

static Node *
uvalue(Node *n, Type *ty)
{
	Node *elt;

	elt = mkexpr(n->loc, Oudata, n, NULL);
	elt->expr.type = ty;
	return elt;
}

static Node *
tupelt(Node *n, size_t i)
{
	Node *idx, *elt;

	idx = mkintlit(n->loc, i);
	idx->expr.type = mktype(n->loc, Tyuint64);
	elt = mkexpr(n->loc, Otupget, n, idx, NULL);
	elt->expr.type = tybase(exprtype(n))->sub[i];
	return elt;
}

static Node *
arrayelt(Node *n, size_t i)
{
	Node *idx, *elt;

	idx = mkintlit(n->loc, i);
	idx->expr.type = mktype(n->loc, Tyuint64);
	elt = mkexpr(n->loc, Oidx, n, idx, NULL);
	elt->expr.type = tybase(exprtype(n))->sub[0];
	return elt;
}

static Node *
findmemb(Node *pat, Node *name)
{
	Node *n;
	size_t i;

	for (i = 0; i < pat->expr.nargs; i++) {
		n = pat->expr.args[i];
		if (nameeq(n->expr.idx, name))
			return n;
	}
	return NULL;
}

static Dtree *
dtbytag(Dtree *t, Ucon *uc)
{
	uint32_t tagval;
	Node *taglit;
	size_t i;

	for (i = 0; i < t->npat; i++) {
		taglit = t->pat[i]->expr.args[0];
		tagval = taglit->lit.intval;
		if (tagval == uc->id) {
			return t->next[i];
		}
	}
	return NULL;
}

static Node *
structmemb(Node *n, Node *name, Type *ty)
{
	Node *elt;

	elt = mkexpr(n->loc, Omemb, n, name, NULL);
	elt->expr.type = ty;
	return elt;
}

static Node *
addcapture(Node *n, Node **cap, size_t ncap)
{
	Node **blk;
	size_t nblk, i;

	nblk = 0;
	blk = NULL;

	for (i = 0; i < ncap; i++)
		lappend(&blk, &nblk, cap[i]);
	for (i = 0; i < n->block.nstmts; i++)
		lappend(&blk, &nblk, n->block.stmts[i]);
	lfree(&n->block.stmts, &n->block.nstmts);
	n->block.stmts = blk;
	n->block.nstmts = nblk;
	return n;
}

static Dtree *
mkdtree(Srcloc loc, Node *lbl)
{
	Dtree *t;

	t = zalloc(sizeof(Dtree));
	t->lbl = lbl;
	t->loc = loc;
	t->id = ndtree++;
	return t;
}

static Dtree *
nextnode(Srcloc loc, size_t idx, size_t count, Dtree *accept)
{
	if (idx == count - 1)
		return accept;
	else
		return mkdtree(loc, genlbl(loc));
}

static size_t
nconstructors(Type *t)
{
	if (!t)
		return 0;

	t = tybase(t);
	switch (t->type) {
	case Tyvoid:	return 1;	break;
	case Tybool:	return 2;	break;
	case Tychar:	return 0x10ffff;	break;

	/* signed ints */
	case Tyint8:	return 0x100;	break;
	case Tyint16:	return 0x10000;	break;
	case Tyint32:	return 0x100000000;	break;
	case Tyint:	return 0x100000000;	break;
	case Tyint64:	return ~0ull;	break;

	/* unsigned ints */
	case Tybyte:	return 0x100;	break;
	case Tyuint8:	return 0x100;	break;
	case Tyuint16:	return 0x10000;	break;
	case Tyuint32:	return 0x100000000;	break;
	case Tyuint:	return 0x100000000;	break;
	case Tyuint64:	return ~0ull;	break;

	/* floats */
	case Tyflt32:	return ~0ull;	break;
	case Tyflt64:	return ~0ull;	break;

	/* complex types */
	case Typtr:	return 1;	break;
	case Tyarray:	return 1;	break;
	case Tytuple:	return 1;	break;
	case Tystruct:  return 1;
	case Tyunion:	return t->nmemb;	break;
	case Tyslice:	return ~0ULL;	break;

	case Tyvar: case Typaram: case Tyunres: case Tyname:
	case Tybad: case Tyvalist: case Tygeneric: case Ntypes:
	case Tyfunc: case Tycode:
		die("Invalid constructor type %s in match", tystr(t));
		break;
	}
	return 0;
}

static int
verifymatch(Dtree *t)
{
	size_t i;
	int ret;

	if (t->accept)
		return 1;

	ret = 0;
	if (t->nnext == t->nconstructors || t->any)
		ret = 1;
	for (i = 0; i < t->nnext; i++)
		if (!verifymatch(t->next[i]))
			ret = 0;
	return ret;
}

static int
acceptall(Dtree *t, Dtree *accept)
{
	size_t i;
	int ret;

	if (t->accept || t->any == accept)
		return 0;

	ret = 1;
	if (t->any)
		ret = acceptall(t->any, accept);
	else
		t->any = accept;

	for (i = 0; i < t->nnext; i++)
		if (acceptall(t->next[i], accept))
			ret = 1;
	return ret;
}

static int
isnonrecursive(Dtree *dt, Type *ty)
{
	if (istyprimitive(ty) || ty->type == Tyvoid || ty->type == Tyfunc || ty->type == Typtr)
		return 1;
	if (ty->type == Tystruct)
		return ty->nmemb == 0;
	if (ty->type == Tyarray)
		return fold(ty->asize, 1)->expr.args[0]->lit.intval == 0;
	return 0;
}

static int
addwildrec(Srcloc loc, Type *ty, Dtree *start, Dtree *accept, Dtree ***end, size_t *nend)
{
	Dtree *next, **last, **tail;
	size_t i, j, nelt, nlast, ntail;
	Node *asize;
	Ucon *uc;
	int ret;

	tail = NULL;
	ntail = 0;
	ty = tybase(ty);
	if (ty->type == Typtr && start->any && start->any->ptrwalk) {
		return addwildrec(loc, ty->sub[0], start->any, accept, end, nend);
	} else if (isnonrecursive(start, ty)) {
		if (start->accept || start == accept)
			return 0;
		for (i = 0; i < start->nnext; i++)
			lappend(end, nend, start->next[i]);
		if (start->any) {
			lappend(end, nend, start->any);
			return 0;
		} else {
			start->any = accept;
			lappend(end, nend, accept);
			return 1;
		}
	}

	ret = 0;
	last = NULL;
	nlast = 0;
	switch (ty->type) {
	case Tytuple:
		lappend(&last, &nlast, start);
		for (i = 0; i < ty->nsub; i++) {
			next = nextnode(loc, i, ty->nsub, accept);
			tail = NULL;
			ntail = 0;
			for (j = 0; j < nlast; j++)
				if (addwildrec(loc, ty->sub[i], last[j], next, &tail, &ntail))
					ret = 1;
			lfree(&last, &nlast);
			last = tail;
			nlast = ntail;
		}
		break;
	case Tyarray:
		lappend(&last, &nlast, start);
		asize = fold(ty->asize, 1);
		nelt = asize->expr.args[0]->lit.intval;
		for (i = 0; i < nelt; i++) {
			next = nextnode(loc, i, nelt, accept);
			tail = NULL;
			ntail = 0;
			for (j = 0; j < nlast; j++)
				if (addwildrec(loc, ty->sub[0], last[j], next, &tail, &ntail))
					ret = 1;
			lfree(&last, &nlast);
			last = tail;
			nlast = ntail;
		}
		break;
	case Tystruct:
		lappend(&last, &nlast, start);
		for (i = 0; i < ty->nmemb; i++) {
			next = nextnode(loc, i, ty->nmemb, accept);
			tail = NULL;
			ntail = 0;
			for (j = 0; j < nlast; j++)
				if (addwildrec(loc, decltype(ty->sdecls[i]), last[j], next, &tail, &ntail))
					ret = 1;
			lfree(&last, &nlast);
			last = tail;
			nlast = ntail;
		}
		break;
	case Tyunion:
		for (i = 0; i < ty->nmemb; i++) {
			uc = ty->udecls[i];
			next = dtbytag(start, uc);
			if (next) {
				if (uc->etype) {
					if (addwildrec(loc, uc->etype, next, accept, end, nend))
						ret = 1;
				} else {
					lappend(end, nend, next);
				}
			}
		}
		if (!start->any) {
			start->any = accept;
			ret = 1;
		}
		lappend(&last, &nlast, accept);
		break;
	case Tyslice:
		ret = acceptall(start, accept);
		lappend(&last, &nlast, accept);
		break;
	default:
		die("unreachable");
	}
	lcat(end, nend, last, nlast);
	lfree(&last, &nlast);
	return ret;
}

static int
addwild(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	Node *asn;

	if (cap && ncap) {
		asn = mkexpr(pat->loc, Oasn, pat, val, NULL);
		asn->expr.type = exprtype(pat);
		lappend(cap, ncap, asn);
	}
	return addwildrec(pat->loc, exprtype(pat), start, accept, end, nend);
}

static int
addunion(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	Node *tagid;
	Dtree *next;
	Ucon *uc;

	if (start->any) {
		lappend(end, nend, start->any);
		return 0;
	}

	uc = finducon(tybase(exprtype(pat)), pat->expr.args[0]);
	next = dtbytag(start, uc);
	if (next) {
		if (!uc->etype) {
			lappend(end, nend, next);
			return 0;
		} else {
			return addpat(pat->expr.args[1], uvalue(val, uc->etype), next, accept, cap, ncap, end, nend);
		}
	}

	if (!start->load) {
		start->load = utag(val);
		start->nconstructors = nconstructors(tybase(exprtype(pat)));
	}

	tagid = mkintlit(pat->loc, uc->id);
	tagid->expr.type = mktype(pat->loc, Tyint32);
	lappend(&start->pat, &start->npat, tagid);
	if (uc->etype) {
		next = mkdtree(pat->loc, genlbl(pat->loc));
		lappend(&start->next, &start->nnext, next);
		addpat(pat->expr.args[1], uvalue(val, uc->etype), next, accept, cap, ncap, end, nend);
	} else {
		lappend(&start->next, &start->nnext, accept);
		lappend(end, nend, accept);
	}
	return 1;
}

static int
addstr(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	Dtree **tail, **last, *next;
	size_t i, j, n, ntail, nlast;
	Node *p, *v, *lit;
	Type *ty;
	char *s;
	int ret;

	lit = pat->expr.args[0];
	n = lit->lit.strval.len;
	s = lit->lit.strval.buf;

	ty = mktype(pat->loc, Tyuint64);
	p = mkintlit(lit->loc, n);
	v = structmemb(val, mkname(pat->loc, "len"), ty);
	p->expr.type = ty;

	if (n == 0)
		next = accept;
	else
		next = mkdtree(pat->loc, genlbl(pat->loc));

	last = NULL;
	nlast = 0;
	ret = 0;
	if (addpat(p, v, start, next, cap, ncap, &last, &nlast))
		ret = 1;

	ty = mktype(pat->loc, Tybyte);
	for (i = 0; i < n; i++) {
		p = mkintlit(lit->loc, s[i]);
		p->expr.type = ty;
		v = arrayelt(val, i);

		tail = NULL;
		ntail = 0;
		next = nextnode(pat->loc, i, n, accept);
		for (j = 0; j < nlast; j++)
			if (addpat(p, v, last[j], next, NULL, NULL, &tail, &ntail))
				ret = 1;
		lfree(&last, &nlast);
		last = tail;
		nlast = ntail;
	}
	lcat(end, nend, last, nlast);
	lfree(&last, &nlast);
	return ret;
}

static int
addlit(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	size_t i;

	if (pat->expr.args[0]->lit.littype == Lstr) {
		return addstr(pat, val, start, accept, cap, ncap, end, nend);
	} else {
		/* if we already have a match, we're not adding a new node */
		if (start->any) {
			lappend(end, nend, start->any);
			return 0;
		}

		for (i = 0; i < start->npat; i++) {
			if (liteq(start->pat[i]->expr.args[0], pat->expr.args[0])) {
				lappend(end, nend, start->next[i]);
				return 0;
			}
		}

		/* wire up an edge from start to 'accept' */
		if (!start->load) {
			start->load = val;
			start->nconstructors = nconstructors(exprtype(pat));
		}
		lappend(&start->pat, &start->npat, pat);
		lappend(&start->next, &start->nnext, accept);
		lappend(end, nend, accept);
		return 1;
	}
}

static int
addtup(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	size_t nargs, nlast, ntail, i, j;
	Dtree *next, **last, **tail;
	Node **args;
	int ret;

	args = pat->expr.args;
	nargs = pat->expr.nargs;
	last = NULL;
	nlast = 0;
	lappend(&last, &nlast, start);
	ret = 0;

	for (i = 0; i < nargs; i++) {
		next = nextnode(args[i]->loc, i, nargs, accept);
		tail = NULL;
		ntail = 0;
		for (j = 0; j < nlast; j++)
			if (addpat(pat->expr.args[i], tupelt(val, i), last[j], next, cap, ncap, &tail, &ntail))
				ret = 1;
		lfree(&last, &nlast);
		last = tail;
		nlast = ntail;
	}
	lcat(end, nend, last, nlast);
	lfree(&last, &nlast);
	return ret;
}

static int
addarr(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	size_t nargs, nlast, ntail, i, j;
	Dtree *next, **last, **tail;
	Node **args;
	int ret;

	args = pat->expr.args;
	nargs = pat->expr.nargs;
	last = NULL;
	nlast = 0;
	lappend(&last, &nlast, start);
	ret = 0;

	for (i = 0; i < nargs; i++) {
		next = nextnode(args[i]->loc, i, nargs, accept);
		tail = NULL;
		ntail = 0;
		for (j = 0; j < nlast; j++)
			if (addpat(pat->expr.args[i], arrayelt(val, i), last[j], next, cap, ncap, &tail, &ntail))
				ret = 1;
		lfree(&last, &nlast);
		last = tail;
		nlast = ntail;
	}
	lcat(end, nend, last, nlast);
	lfree(&last, &nlast);
	return ret;
}

static int
addstruct(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	Dtree *next, **last, **tail;
	Node *memb, *name;
	Type *ty, *mty;
	size_t i, j, ntail, nlast;
	int ret;

	ret = 0;
	last = NULL;
	nlast = 0;
	lappend(&last, &nlast, start);
	ty = tybase(exprtype(pat));

	for (i = 0; i < ty->nmemb; i++) {
		mty = decltype(ty->sdecls[i]);
		name = ty->sdecls[i]->decl.name;
		memb = findmemb(pat, name);
		next = nextnode(pat->loc, i, ty->nmemb, accept);
		tail = NULL;
		ntail = 0;

		/* add a _ capture if we don't specify the value */
		if (!memb) {
			memb = mkexpr(ty->sdecls[i]->loc, Ogap, NULL);
			memb->expr.type = mty;
		}
		for (j = 0; j < nlast; j++) {
			if (addpat(memb, structmemb(val, name, mty), last[j], next, cap, ncap, &tail, &ntail))
				ret = 1;
		}
		lfree(&last, &nlast);
		last = tail;
		nlast = ntail;
	}
	lcat(end, nend, last, nlast);
	lfree(&last, &nlast);
	return ret;
}

static int
addderefpat(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	Node *deref;
	Dtree *walk;

	deref = mkexpr(val->loc, Oderef, val, NULL);
	deref->expr.type = exprtype(pat->expr.args[0]);
	start->nconstructors = nconstructors(exprtype(deref));
	if (start->any && !start->any->ptrwalk)
		return 0;
	else if (!start->any)
		start->any = mkdtree(pat->loc, genlbl(pat->loc));
	walk = start->any;
	walk->ptrwalk = 1;
	return addpat(pat->expr.args[0], deref, walk, accept, cap, ncap, end, nend);
}

static int
addpat(Node *pat, Node *val, Dtree *start, Dtree *accept, Node ***cap, size_t *ncap, Dtree ***end, size_t *nend)
{
	int ret;
	Node *dcl;
	Type *ty;

	pat = fold(pat, 1);
	ret = 0;
	switch (exprop(pat)) {
	case Ovar:
		dcl = decls[pat->expr.did];
		if (dcl->decl.isconst) {
			ty = decltype(dcl);
			if (ty->type == Tyfunc || ty->type == Tycode || ty->type == Tyvalist)
				fatal(dcl, "bad pattern %s:%s: unmatchable type", declname(dcl), tystr(ty));
			if (!dcl->decl.init)
				fatal(dcl, "bad pattern %s:%s: missing initializer", declname(dcl), tystr(ty));
			ret = addpat(dcl->decl.init, val, start, accept, cap, ncap, end, nend);
		} else {
			ret = addwild(pat, val, start, accept, cap, ncap, end, nend);
		}
		break;
	case Oucon:
		ret = addunion(pat, val, start, accept, cap, ncap, end, nend);
		break;
	case Olit:
		ret = addlit(pat, val, start, accept, cap, ncap, end, nend);
		break;
	case Otup:
		ret = addtup(pat, val, start, accept, cap, ncap, end, nend);
		break;
	case Oarr:
		ret = addarr(pat, val, start, accept, cap, ncap, end, nend);
		break;
	case Ostruct:
		ret = addstruct(pat, val, start, accept, cap, ncap, end, nend);
		break;
	case Ogap:
		ret = addwild(pat, NULL, start, accept, NULL, NULL, end, nend);
		break;
	case Oaddr:
		ret = addderefpat(pat, val, start, accept, cap, ncap, end, nend);
		break;
	default:
		fatal(pat, "unsupported pattern %s of type %s", opstr[exprop(pat)], tystr(exprtype(pat)));
		break;
	}
	return ret;
}


/* val must be a pure, fully evaluated value */
Dtree *
gendtree(Node *m, Node *val, Node **lbl, size_t nlbl, int startid)
{
	Dtree *start, *accept, **end;
	Node **pat, **cap;
	size_t npat, ncap, nend;
	size_t i;

	ndtree = startid;

	pat = m->matchstmt.matches;
	npat = m->matchstmt.nmatches;

	end = NULL;
	nend = 0;
	start = mkdtree(m->loc, genlbl(m->loc));
	for (i = 0; i < npat; i++) {
		cap = NULL;
		ncap = 0;
		accept = mkdtree(lbl[i]->loc, lbl[i]);
		accept->accept = 1;

		if (!addpat(pat[i]->match.pat, val, start, accept, &cap, &ncap, &end, &nend))
			fatal(pat[i], "pattern matched by earlier case");
		addcapture(pat[i]->match.block, cap, ncap);
	}
	if (debugopt['M'])
		dtreedump(stdout, start);
	if (!verifymatch(start))
		fatal(m, "nonexhaustive pattern set in match statement");
	return start;
}

void
genmatchcode(Dtree *dt, Node ***out, size_t *nout)
{
	Node *jmp, *eq, *fail;
	int emit;
	size_t i;

	if (dt->emitted)
		return;
	dt->emitted = 1;

	/* the we jump to the accept label when generating the parent */
	if (dt->accept) {
		jmp = mkexpr(dt->loc, Ojmp, dt->lbl, NULL);
		jmp->expr.type = mktype(dt->loc, Tyvoid);
		lappend(out, nout, jmp);
		return;
	}

	lappend(out, nout, dt->lbl);
	for (i = 0; i < dt->nnext; i++) {
		if (i == dt->nnext - 1 && dt->any) {
			fail = dt->any->lbl;
			emit = 0;
		} else {
			fail = genlbl(dt->loc);
			emit = 1;
		}

		eq = mkexpr(dt->loc, Oeq, dt->load, dt->pat[i], NULL);
		eq->expr.type = mktype(dt->loc, Tybool);
		jmp = mkexpr(dt->loc, Ocjmp, eq, dt->next[i]->lbl, fail, NULL);
		jmp->expr.type = mktype(dt->loc, Tyvoid);
		lappend(out, nout, jmp);

		genmatchcode(dt->next[i], out, nout);
		if (emit)
			lappend(out, nout, fail);
	}
	if (dt->any) {
		jmp = mkexpr(dt->loc, Ojmp, dt->any->lbl, NULL);
		jmp->expr.type = mktype(dt->loc, Tyvoid);
		lappend(out, nout, jmp);
		genmatchcode(dt->any, out, nout);
	}
}

void
genonematch(Node *pat, Node *val, Node *iftrue, Node *iffalse, Node ***out, size_t *nout, Node ***cap, size_t *ncap)
{
	Dtree *start, *accept, *reject, **end;
	size_t nend;

	end = NULL;
	nend = 0;

	start = mkdtree(pat->loc, genlbl(pat->loc));
	accept = mkdtree(iftrue->loc, iftrue);
	accept->accept = 1;
	reject = mkdtree(iffalse->loc, iffalse);
	reject->accept = 1;

	assert(addpat(pat, val, start, accept, cap, ncap, &end, &nend));
	acceptall(start, reject);
	genmatchcode(start, out, nout);
}

void
genmatch(Node *m, Node *val, Node ***out, size_t *nout)
{
	Node **pat, **lbl, *end, *endlbl;
	size_t npat, nlbl, i;
	Dtree *dt;

	lbl = NULL;
	nlbl = 0;

	pat = m->matchstmt.matches;
	npat = m->matchstmt.nmatches;
	for (i = 0; i < npat; i++)
		lappend(&lbl, &nlbl, genlbl(pat[i]->match.block->loc));


	endlbl = genlbl(m->loc);
	if (getenv("OLD_MATCH")) {
		dt = gendtree(m, val, lbl, nlbl, ndtree);
	} else {
		dt = gendtree2(m, val, lbl, nlbl, ndtree);
	}
	genmatchcode(dt, out, nout);

	for (i = 0; i < npat; i++) {
		end = mkexpr(pat[i]->loc, Ojmp, endlbl, NULL);
		end->expr.type = mktype(end->loc, Tyvoid);
		lappend(out, nout, lbl[i]);
		lappend(out, nout, pat[i]->match.block);
		lappend(out, nout, end);
	}
	lappend(out, nout, endlbl);

	if (debugopt['m']) {
		dtreedump(stdout, dt);
		for (i = 0; i < *nout; i++)
			dumpn((*out)[i], stdout);
	}
}


void
dtreedumplit(FILE *fd, Dtree *dt, Node *n, size_t depth)
{
	char *s;

	s = lblstr(dt->lbl);
	switch (n->lit.littype) {
	case Lvoid:	findentf(fd, depth, "%s: Lvoid\n");	break;
	case Lchr:	findentf(fd, depth, "%s: Lchr %c\n", s, n->lit.chrval);	break;
	case Lbool:	findentf(fd, depth, "%s: Lbool %s\n", s, n->lit.boolval ? "true" : "false");	break;
	case Lint:	findentf(fd, depth, "%s: Lint %llu\n", s, n->lit.intval);	break;
	case Lflt:	findentf(fd, depth, "%s: Lflt %lf\n", s, n->lit.fltval);	break;
	case Lstr:	findentf(fd, depth, "%s: Lstr %.*s\n", s, (int)n->lit.strval.len, n->lit.strval.buf);	break;
	case Llbl:	findentf(fd, depth, "%s: Llbl %s\n", s, n->lit.lblval);	break;
	case Lfunc:	findentf(fd, depth, "%s: Lfunc\n");	break;
	}
}

void
dtreedumpnode(FILE *fd, Dtree *dt, size_t depth)
{
	size_t i;

	if (dt->accept)
		findentf(fd, depth, "%s: accept\n", lblstr(dt->lbl));

	for (i = 0; i < dt->nnext; i++) {
		dtreedumplit(fd, dt, dt->pat[i]->expr.args[0], depth);
		dtreedumpnode(fd, dt->next[i], depth + 1);
	}
	if (dt->any) {
		findentf(fd, depth, "%s: wildcard\n", lblstr(dt->lbl));
		dtreedumpnode(fd, dt->any, depth + 1);
	}
}

void
dtreedump(FILE *fd, Dtree *dt)
{
	dtreedumpnode(fd, dt, 0);
}


typedef struct Path {
	unsigned char len;
	unsigned char *p;
} Path;

typedef struct Slot {
	Path *path;
	Node *pat;
	Node *load;
} Slot;

static Slot *
newslot(Path *path, Node *pat, Node *val)
{
	Slot *s;
	s = zalloc(sizeof(Slot));
	s->path = path;
	s->pat = pat;
	s->load = val;
	assert(path != (void *)1);
	return s;
}

// The instances of the struct are immutable.
typedef struct Frontier {
	int i;
	Node *lbl;
	Slot  **slot;
	size_t nslot;
	Node **cap;
	size_t ncap;
} Frontier;

static Path*
newpath(Path *p, char c)
{
	Path *newp;

	newp = zalloc(sizeof(Path));
	if (p) {
		newp->p = zalloc(p->len+1);
		memcpy(newp->p, p->p, p->len);
		newp->p[p->len] = c;
		newp->len = p->len+1;
	} else {
		newp->p = zalloc(8);
		newp->p[0] = 0;
		newp->len = 1;
	}
	assert(newp->p != 0);
	return newp;
}

static int
patheq(Path *a, Path *b)
{
	unsigned char i;

	if (a->len != b->len) {
		return 0;
	}
	for (i = 0; i < a->len; i++) {
		if (a->p[i] != b->p[i]) {
			return 0;
		}
	}
	return 1;
}

static int
pateq(Node *a, Node *b)
{
	if (exprop(a) != exprop(b)) {
		return 0;
	}
	switch (exprop(a)) {
	case Olit:
		return liteq(a->expr.args[0], b->expr.args[0]);
	case Ogap:
	case Ovar:
		return 1;
	default:
		assert(0);
	}
}

char *
pathfmt(Path *p)
{
	size_t i, sz, n;
	char *buf;

	sz = p->len*3+1;
	buf = zalloc(sz);
	n = 0;
	for (i = 0; i < p->len; i++) {
		n += snprintf(&buf[n], sz-n, "%02x,", p->p[i]);
	}
	buf[n-1] = '\0';
	return buf;
}

void
pathdump(Path *p, FILE *out)
{
	size_t i;
	for (i = 0; i < p->len; i++) {
		fprintf(out, "%x,", p->p[i]);
	}
	fprintf(out, "\n");
}

static void
addrec(Frontier *fs, Node *pat, Node *val, Path *path)
{
	size_t i, n;
	Type *ty, *mty;
	Node *memb, *name, *tagid, *p, *v, *lit, *dcl, *asn, *deref;
	Ucon *uc;
	char *s;

	pat = fold(pat, 1);
	switch (exprop(pat)) {
	case Ogap:
		lappend(&fs->slot, &fs->nslot, newslot(path, pat, val));
		break;
	case Ovar:
		dcl = decls[pat->expr.did];
		if (dcl->decl.isconst) {
			ty = decltype(dcl);
			if (ty->type == Tyfunc || ty->type == Tycode || ty->type == Tyvalist) {
				fatal(dcl, "bad pattern %s:%s: unmatchable type", declname(dcl), tystr(ty));
			}
			if (!dcl->decl.init) {
				fatal(dcl, "bad pattern %s:%s: missing initializer", declname(dcl), tystr(ty));
			}
			addrec(fs, dcl->decl.init, val, newpath(path, 0));
		} else {
			asn = mkexpr(pat->loc, Oasn, pat, val, NULL);
			asn->expr.type = exprtype(pat);
			lappend(&fs->cap, &fs->ncap, asn);

			lappend(&fs->slot, &fs->nslot, newslot(path, pat, val));
		}
		break;
	case Olit:
		if (pat->expr.args[0]->lit.littype == Lstr) {
			lit = pat->expr.args[0];
			n = lit->lit.strval.len;
			s = lit->lit.strval.buf;

			ty = mktype(pat->loc, Tyuint64);
			p = mkintlit(lit->loc, n);
			p ->expr.type = ty;
			v = structmemb(val, mkname(pat->loc, "len"), ty);

			addrec(fs, p, v, newpath(path, 0));

			ty = mktype(pat->loc, Tybyte);
			for (i = 0; i < n; i++) {
				p = mkintlit(lit->loc, s[i]);
				p->expr.type = ty;
				v = arrayelt(val, i);
				addrec(fs, p, v, newpath(path, 1+i));
			}
		} else {
			lappend(&fs->slot, &fs->nslot, newslot(path, pat, val));
		}
		break;
	case Oaddr:
		deref = mkexpr(val->loc, Oderef, val, NULL);
		deref->expr.type = exprtype(pat->expr.args[0]);
		addrec(fs, pat->expr.args[0], deref, newpath(path, 0));
		break;
	case Oucon:
		uc = finducon(tybase(exprtype(pat)), pat->expr.args[0]);
		tagid = mkintlit(pat->loc, uc->id);
		tagid->expr.type = mktype(pat->loc, Tyint32);
		addrec(fs, tagid, utag(val), newpath(path, 0));
		if (uc->etype) {
			addrec(fs, pat->expr.args[1], uvalue(val, uc->etype), newpath(path, 1));
		}
		break;
	case Otup:
		for (i = 0; i < pat->expr.nargs; i++) {
			addrec(fs, pat->expr.args[i], tupelt(val, i), newpath(path, i));
		}
		break;
	case Oarr:
		for (i = 0; i < pat->expr.nargs; i++) {
			addrec(fs, pat->expr.args[i], arrayelt(val, i), newpath(path, i));
		}
		break;
	case Ostruct:
		ty = tybase(exprtype(pat));
		for (i = 0; i < ty->nmemb; i++) {
			mty = decltype(ty->sdecls[i]);
			name = ty->sdecls[i]->decl.name;
			memb = findmemb(pat, name);
			if (!memb) {
				memb = mkexpr(ty->sdecls[i]->loc, Ogap, NULL);
				memb->expr.type = mty;
			}
			addrec(fs, memb, structmemb(val, name, mty), newpath(path, i));
		}
		break;
	default:
		fatal(pat, "unsupported pattern %s of type %s", opstr[exprop(pat)], tystr(exprtype(pat)));
		break;
	}
}

static void
genfrontier(int i, Node *val, Node *pat, Node *lbl, Frontier ***frontier, size_t *nfrontier)
{
	Frontier *fs;

	fs = zalloc(sizeof(Frontier));
	fs->i = i;
	fs->lbl = lbl;
	addrec(fs, pat, val, newpath(NULL, 0));
	lappend(frontier, nfrontier, fs);
}

// project updates the input frontier by producing a new the set of slots.
static Frontier *
project(Node *pat, Path *pi, Node *val, Frontier *fs)
{
	Frontier *_fs;
	Slot *c, **slot;
	size_t i, nslot;

	assert (fs->nslot > 0);

	// select the current frontier when the sub-term val does not present in the frontier fs
	c = NULL;
	slot = NULL;
	nslot = 0;
	for (i = 0; i < fs->nslot; i++) {
		if (patheq(pi, fs->slot[i]->path)) {
			c = fs->slot[i];
			continue;
		}
		lappend(&slot, &nslot, fs->slot[i]);
	}

	_fs = zalloc(sizeof(Frontier));
	_fs->i = fs->i;
	_fs->lbl = fs->lbl;
	_fs->slot = slot;
	_fs->nslot = nslot;
	_fs->cap = fs->cap;
	_fs->ncap = fs->ncap;

	// if the sub-term at pi is not in the frontier,
	// then we do not reduce the frontier.
	if (c == NULL) {
		return _fs;
	}

	switch (exprop(c->pat)) {
	case Ovar:
	case Ogap:
		// if the pattern at the sub-term pi of this frontier is not a constructor,
		// then we do not reduce the frontier.
		return _fs;
	default:
		break;
	}

	// if constructor at the path pi is not the constructor we want to project,
	// then return null.
	if (!pateq(pat, c->pat)) {
		return NULL;
	}

	return _fs;
}

static Dtree *
compile(Frontier **frontier, size_t nfrontier)
{
	size_t i, j, k;
	Dtree *dt, *_dt, **edge, *any;
	Frontier *fs, *_fs, **_frontier, **defaults ;
	Node **cs, **_pat;
	Slot *slot, *s;
	size_t ncs, ncons, _nfrontier, nedge, ndefaults, _npat;

	assert(nfrontier > 0);

	fs = frontier[0];

	// scan constructors horizontally
	ncons = 0;
	for (i = 0; i < fs->nslot; i++) {
		switch (exprop(fs->slot[i]->pat)) {
		case Ovar:
		case Ogap:
			break;
		default:
			ncons++;
		}
	}
	if (ncons == 0) {
		dt = mkdtree(fs->lbl->loc, fs->lbl);
		dt->accept = 1;
		return dt;
	}

	assert(fs->nslot > 0);

	// always select the first found constructor
	slot = NULL;
	for (i = 0; i < fs->nslot; i++) {
		switch (exprop(fs->slot[i]->pat)) {
		case Ovar:
		case Ogap:
			continue;
		default:
			slot = fs->slot[i];
			goto pi_found;
		}
	}

pi_found:
	// scan constructors vertically at pi to create the set 'CS'
	cs = NULL;
	ncs = 0;
	for (i = 0; i < nfrontier; i++) {
		fs = frontier[i];
		for (j = 0; j < fs->nslot; j++) {
			s = fs->slot[j];
			switch (exprop(s->pat)) {
			case Ovar:
			case Ogap:
				break;
			case Olit:
				if (patheq(slot->path, s->path)) {

					// Look for a duplicate entry; we want a unique set of cs.
					// We could use a hash table, but given that the n is usually small,
					// an exhaustive search suffices.
					for (k = 0; k < ncs; k++) {
						if (pateq(cs[k], s->pat)) {
							break;
						}
					}
					if (ncs == 0 || k == ncs) {
						lappend(&cs, &ncs, s->pat);
					}
				}
				break;
			default:
				assert(0);
			}
		}
	}
	// Project a new frontier for each selected constructor
	edge = NULL;
	nedge = 0;
	_pat = NULL;
	_npat = 0;
	for (i = 0; i < ncs; i++) {
		_frontier = NULL;
		_nfrontier = 0;
		for (j = 0; j < nfrontier; j++) {
			fs = frontier[j];
			_fs = project(cs[i], slot->path, slot->load, fs);
			if (_fs != NULL) {
				lappend(&_frontier, &_nfrontier, _fs);
			}
		}

		if (_nfrontier > 0) {
			dt = compile(_frontier, _nfrontier);
			lappend(&edge, &nedge, dt);
			lappend(&_pat, &_npat, cs[i]);
		}
	}

	// compile the defaults
	defaults = NULL;
	ndefaults = 0;
	for (i = 0; i < nfrontier; i++) {
		fs = frontier[i];
		k = -1;
		for (j = 0; j < fs->nslot; j++) {
			// locate the occurrence of pi in fs
			if (patheq(slot->path, fs->slot[j]->path)) {
				k = j;
			}
		}

		if (k == -1 || exprop(fs->slot[k]->pat) == Ovar || exprop(fs->slot[k]->pat) == Ogap) {
			lappend(&defaults, &ndefaults, fs);
		}
	}
	if (ndefaults) {
		any = compile(defaults, ndefaults);
	} else {
		any = NULL;
	}

	// construct the result dtree
	_dt = mkdtree(slot->pat->loc, genlbl(slot->pat->loc));
	_dt->load = slot->load;
	_dt->npat = _npat,
	_dt->pat = _pat,
	_dt->nnext = nedge;
	_dt->next = edge;
	_dt->any = any;

	return _dt;
}


Dtree *
gendtree2(Node *m, Node *val, Node **lbl, size_t nlbl, int startid)
{
	Dtree *root;
	Node **pat;
	size_t npat;
	size_t i;
	Frontier **frontier;
	size_t nfrontier;


	ndtree = startid;

	pat = m->matchstmt.matches;
	npat = m->matchstmt.nmatches;

	frontier = NULL;
	nfrontier = 0;
	for (i = 0; i < npat; i++) {
		genfrontier(i, val, pat[i]->match.pat, lbl[i], &frontier, &nfrontier);
	}
	for (i = 0; i < nfrontier; i++) {
		addcapture(pat[i]->match.block, frontier[i]->cap, frontier[i]->ncap);
	}
	root = compile(frontier, nfrontier);

	if (debugopt['M'] || getenv("M")) {
		dtreedump(stdout, root);
	}

	return root;
}


