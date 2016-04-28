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

static size_t blobrec(Blob *b, Htab *globls, Htab *strtab, Node *n);

Blob *mkblobi(Blobtype type, uint64_t ival)
{
	Blob *b;

	b = zalloc(sizeof(Blob));
	b->type = type;
	b->ival = ival;
	return b;
}

Blob *mkblobpad(size_t sz)
{
	Blob *b;

	b = zalloc(sizeof(Blob));
	b->type = Btpad;
	b->npad = sz;
	return b;
}

Blob *mkblobbytes(char *buf, size_t len)
{
	Blob *b;

	b = zalloc(sizeof(Blob));
	b->type = Btbytes;
	b->bytes.buf = buf;
	b->bytes.len = len;
	return b;
}

Blob *mkblobseq(Blob **sub, size_t nsub)
{
	Blob *b;

	b = zalloc(sizeof(Blob));
	b->type = Btseq;
	b->seq.sub = sub;
	b->seq.nsub = nsub;
	return b;
}

Blob *mkblobref(char *lbl, size_t off, int isextern)
{
	Blob *b;

	b = zalloc(sizeof(Blob));
	b->type = Btref;
	b->ref.str = strdup(lbl);
	b->ref.isextern = isextern;
	b->ref.off = off;
	return b;
}

void blobfree(Blob *b)
{
	size_t i;

	if (!b)
		return;
	switch (b->type) {
	case Btref:
		free(b->lbl);
		break;
	case Btseq:
		for (i = 0; i < b->seq.nsub; i++)

			blobfree(b->seq.sub[i]);
		break;
	default:
		break;
	}
	free(b);
}

static size_t getintlit(Node *n, char *failmsg)
{
	if (exprop(n) != Olit)
		fatal(n, "%s", failmsg);
	n = n->expr.args[0];
	if (n->lit.littype != Lint)
		fatal(n, "%s", failmsg);
	return n->lit.intval;
}

void b(Blob *b, Blob *n)
{
	lappend(&b->seq.sub, &b->seq.nsub, n);
}

static size_t blobpad(Blob *seq, size_t sz)
{
	if (sz)
		b(seq, mkblobpad(sz));
	return sz;
}

static size_t bloblit(Blob *seq, Htab *strtab, Node *v, Type *ty)
{
	char buf[128];
	char *lbl;
	size_t sz;
	Blobtype intsz[] = {
		[1] = Bti8,
		[2] = Bti16,
		[4] = Bti32,
		[8] = Bti64
	};
	union {
		float fv;
		double dv;
		uint64_t qv;
		uint32_t lv;
	} u;

	assert(v->type == Nlit);
	sz = tysize(ty);
	switch (v->lit.littype) {
	case Lvoid:	break;
	case Lint:	b(seq, mkblobi(intsz[sz], v->lit.intval));	break;
	case Lbool:	b(seq, mkblobi(Bti8, v->lit.boolval));	break;
	case Lchr:	b(seq, mkblobi(Bti32, v->lit.chrval));	break;
	case Lflt:
		if (tybase(v->lit.type)->type == Tyflt32) {
			u.fv = v->lit.fltval;
			b(seq, mkblobi(Bti32, u.lv));
		} else if (tybase(v->lit.type)->type == Tyflt64) {
			u.dv = v->lit.fltval;
			b(seq, mkblobi(Bti64, u.qv));
		}
		break;
	case Lstr:
		if (hthas(strtab, &v->lit.strval)) {
			lbl = htget(strtab, &v->lit.strval);
		} else {
			lbl = genlblstr(buf, sizeof buf, "");
			htput(strtab, &v->lit.strval, strdup(lbl));
		}
		if (v->lit.strval.len > 0)
			b(seq, mkblobref(lbl, 0, 1));
		else
			b(seq, mkblobi(Bti64, 0));
		b(seq, mkblobi(Bti64, v->lit.strval.len));
		break;
	case Lfunc:
		die("Generating this shit ain't ready yet ");
		break;
	case Llbl:
		die("Can't generate literal labels, ffs. They're not data.");
		break;
	}
	return sz;
}

static size_t blobslice(Blob *seq,  Htab *globls, Htab *strtab, Node *n)
{
	Node *base, *lo, *hi;
	ssize_t loval, hival, sz;
	Blob *slbase;
	char *lbl;

	base = n->expr.args[0];
	lo = n->expr.args[1];
	hi = n->expr.args[2];

	/* by this point, all slicing operations should have had their bases
	 * pulled out, and we should have vars with their pseudo-decls in their
	 * place */
	loval = getintlit(lo, "lower bound in slice is not constant literal");
	hival = getintlit(hi, "upper bound in slice is not constant literal");
	if (exprop(base) == Ovar && base->expr.isconst) {
		sz = tysize(tybase(exprtype(base))->sub[0]);
		lbl = htget(globls, base);
		slbase = mkblobref(lbl, loval*sz, 1);
	} else if (exprop(base) == Olit) {
		slbase = mkblobi(Bti64, getintlit(base, "invalid base expr"));
	} else {
		fatal(base, "slice base is not a constant value");
	}

	b(seq, slbase);
	b(seq, mkblobi(Bti64, (hival - loval)));
	return 16;
}

static Node *structmemb(Node *n, char *dcl)
{
	size_t i;

	for (i = 0; i < n->expr.nargs; i++)
		if (!strcmp(namestr(n->expr.args[i]->expr.idx), dcl))
			return n->expr.args[i];
	return NULL;
}

static size_t blobstruct(Blob *seq, Htab *globls, Htab *strtab, Node *n)
{
	size_t i, sz, pad, end, ndcl;
	Node **dcl, *m;
	Type *t;

	sz = 0;
	t = tybase(exprtype(n));
	assert(t->type == Tystruct);
	dcl = t->sdecls;
	ndcl = t->nmemb;

	for (i = 0; i < ndcl; i++) {
		pad = alignto(sz, decltype(dcl[i]));
		m = structmemb(n, declname(dcl[i]));
		sz += blobpad(seq, pad - sz);
		if (m)
			sz += blobrec(seq, globls, strtab, m);
		else
			sz += blobpad(seq, size(dcl[i]));
	}
	end = alignto(sz, t);
	sz += blobpad(seq, end - sz);
	return sz;
}

static size_t blobucon(Blob *seq, Htab *globls, Htab *strtab, Node *n)
{
	size_t sz, pad;
	Ucon *uc;

	sz = 4;
	uc = finducon(exprtype(n), n->expr.args[0]);
	b(seq, mkblobi(Bti32, uc->id));
	if (n->expr.nargs > 1) {
		pad = tyalign(exprtype(n->expr.args[1])) - sz;
		sz += blobpad(seq, pad);
		sz += blobrec(seq, globls, strtab, n->expr.args[1]);
	}
	sz += blobpad(seq, size(n) - sz);
	return sz;
}


static size_t blobrec(Blob *b, Htab *globls, Htab *strtab, Node *n)
{
	size_t i, sz;

	switch(exprop(n)) {
	case Oucon:	sz = blobucon(b, globls, strtab, n);	break;
	case Oslice:	sz = blobslice(b, globls, strtab, n);	break;
	case Ostruct:	sz = blobstruct(b, globls, strtab, n);	break;
	case Olit:	sz = bloblit(b, strtab, n->expr.args[0], exprtype(n));	break;
	case Otup:
	case Oarr:
		/* Assumption: We sorted this while folding */
		sz = 0;
		for (i = 0; i < n->expr.nargs; i++)
			sz += blobrec(b, globls, strtab, n->expr.args[i]);
		break;
	default:
		dump(n, stdout);
		die("Nonliteral initializer for global");
		break;
	}
	return sz;
}

Blob *litblob(Htab *globls, Htab *strtab, Node *n)
{
	Blob *b;

	b = mkblobseq(NULL, 0);
	blobrec(b, globls, strtab, n);
	return b;
}
