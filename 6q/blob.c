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

static void b(Blob *b, Blob *n)
{
	lappend(&b->seq.sub, &b->seq.nsub, n);
}

static size_t blobpad(Blob *seq, size_t sz)
{
	if (sz)
		b(seq, mkblobpad(sz));
	return sz;
}

static size_t bloblit(Gen *g, Blob *seq, Node *n, Type *ty)
{
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
	char buf[128];
	char *lbl;
	size_t sz;
	Node *v;

	assert(exprop(n) == Olit);
	v = n->expr.args[0];
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
		/* 
		FIXME: dedup strings.
		if (hthas(strtab, &v->lit.strval)) {
			lbl = htget(strtab, &v->lit.strval);
		} else {
			lbl = genlocallblstr(buf, sizeof buf);
			htput(strtab, &v->lit.strval, strdup(lbl));
		}
		*/
		lbl = genlblstr(buf, sizeof buf, "");
		if (v->lit.strval.len > 0)
			b(seq, mkblobref(lbl, 0, 1));
		else
			b(seq, mkblobi(Bti64, 0));
		b(seq, mkblobi(Bti64, v->lit.strval.len));
		break;
	case Lfunc:
		fatal(n, "Generating this shit ain't ready yet");
		break;
	case Llbl:
		die("Can't generate literal labels, ffs. They're not data.");
		break;
	}
	return sz;
}

static size_t blobslice(Gen *g, Blob *seq, Node *n)
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
		lbl = htget(g->globls, base);
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

static size_t blobstruct(Gen *g, Blob *seq, Node *n)
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
			sz += blobrec(g, seq, m);
		else
			sz += blobpad(seq, size(dcl[i]));
	}
	end = alignto(sz, t);
	sz += blobpad(seq, end - sz);
	return sz;
}

static size_t blobucon(Gen *g, Blob *seq, Node *n)
{
	size_t sz, pad;
	Ucon *uc;

	sz = 4;
	uc = finducon(exprtype(n), n->expr.args[0]);
	b(seq, mkblobi(Bti32, uc->id));
	if (n->expr.nargs > 1) {
		pad = tyalign(exprtype(n->expr.args[1])) - sz;
		sz += blobpad(seq, pad);
		sz += blobrec(g, seq, n->expr.args[1]);
	}
	sz += blobpad(seq, size(n) - sz);
	return sz;
}

size_t blobrec(Gen *g, Blob *b, Node *n)
{
	size_t i, sz, end;
	Type *ty;

	ty = exprtype(n);
	switch(exprop(n)) {
	case Oucon:	sz = blobucon(g, b, n);		break;
	case Oslice:	sz = blobslice(g, b, n);	break;
	case Ostruct:	sz = blobstruct(g, b, n);	break;
	case Olit:	sz = bloblit(g, b, n, ty);	break;
	case Otup:
	case Oarr:
		/* Assumption: We sorted this while folding */
		sz = 0;
		if (!n->expr.args)
			break;
		for (i = 0; i < n->expr.nargs; i++) {
			end = alignto(sz, exprtype(n->expr.args[i]));
			sz += blobpad(b, end - sz);
			sz += blobrec(g, b, n->expr.args[i]);
		}
		/* if we need padding at the end.. */
		end = alignto(sz, exprtype(n));
		sz += blobpad(b, end - sz);
		break;
	default:
		dump(n, stdout);
		die("Nonliteral initializer for global");
		break;
	}
	return sz;
}


static void encodemin(Gen *g, uint64_t val)
{
	size_t i, shift;
	uint8_t b;

	if (val < 128) {
		fprintf(g->f, "\tb %lld,\n", (long long)val);
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
	case Bti8:	fprintf(g->f, "\tb %lld,\n", (long long)b->ival);	break;
	case Bti16:	fprintf(g->f, "\th %lld,\n", (long long)b->ival);	break;
	case Bti32:	fprintf(g->f, "\tw %lld,\n", (long long)b->ival);	break;
	case Bti64:	fprintf(g->f, "\tl %lld,\n", (long long)b->ival);	break;
	case Btbytes:	outbytes(g, b->bytes.buf, b->bytes.len);	break;
	case Btpad:	fprintf(g->f, "\tz %lld,\n", (long long)b->npad);	break;
	case Btref:	fprintf(g->f, "\tl $%s + %lld,\n", b->ref.str, (long long)b->ref.off);	break;
	case Btseq:
		for (i = 0; i < b->seq.nsub; i++)
			genblob(g, b->seq.sub[i]);
		break;
	}
	if(b->lbl) {
		fprintf(g->f, "}\n\n");
	}
}

