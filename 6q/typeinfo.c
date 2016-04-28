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

#define Tdindirect 0x80

Blob *tydescsub(Type *ty);

size_t alignto(size_t sz, Type *t)
{
	return align(sz, tyalign(t));
}

size_t blobsz(Blob *b)
{
	size_t n;
	size_t i;

	switch (b->type) {
	case Bti8:	return 1;	break;
	case Bti16:	return 2;	break;
	case Bti32:	return 4;	break;
	case Bti64:	return 8;	break;
	case Btref:	return 8;	break;
	case Btbytes:	return b->bytes.len;	break;
	case Btimin:
		if (b->ival >= 1ULL << 56)
			die("packed int too big");

		for (i = 1; i < 8; i++)
			if (b->ival < 1ULL << (7*i))
				return i;
		die("impossible blob size");
		break;
	case Btseq:
		n = 0;
		for (i = 0; i < b->seq.nsub; i++)
			n += blobsz(b->seq.sub[i]);
		return n;
		break;
	default:
			die("unknown blob type");
	}
	return 0;
}

void namevec(Blob ***sub, size_t *nsub, Node *n)
{
	char *buf;
	size_t len;

	if (n->name.ns) {
		len = strlen(n->name.name) + strlen(n->name.ns) + 1;
		buf = xalloc(len + 1);
		bprintf(buf, len + 1, "%s.%s", n->name.ns, n->name.name);
	} else {
		len = strlen(n->name.name);
		buf = xalloc(len + 1);
		bprintf(buf, len + 1, "%s", n->name.name);
	}
	lappend(sub, nsub, mkblobi(Btimin, len));
	lappend(sub, nsub, mkblobbytes(buf, len));
}

static void structmemb(Blob ***sub, size_t *nsub, Node *sdecl)
{
	Blob *b;

	namevec(sub, nsub, sdecl->decl.name);
	b = tydescsub(sdecl->decl.type);
	lappend(sub, nsub, b);
}

static void unionmemb(Blob ***sub, size_t *nsub, Ucon *ucon)
{
	namevec(sub, nsub, ucon->name);
	if (ucon->etype) {
		lappend(sub, nsub, tydescsub(ucon->etype));
	} else  {
		lappend(sub, nsub, mkblobi(Btimin, 1));
		lappend(sub, nsub, mkblobi(Bti8, Tybad));
	}
}

static void encodetypeinfo(Blob ***sub, size_t *nsub, Type *t)
{
	lappend(sub, nsub, mkblobi(Btimin, tysize(t)));
	lappend(sub, nsub, mkblobi(Btimin, tyalign(t)));
}

Blob *tydescsub(Type *ty)
{
	Blob **sub, *sz, *bt, *b;
	size_t i, nsub;
	char buf[512];
	uint8_t tt;
	Node *len;
	int isextern;

	sub = NULL;
	nsub = 0;
	/* names are pulled out of line */
	tt = ty->type;
	/* tyvars can get tagged, but aren't desired */
	if (ty->type == Tyvar)
		return NULL;

	ty = tydedup(ty);
	if (ty->type == Tyname)
		tt |= Tdindirect;
	sz = mkblobi(Btimin, 0);
	bt = mkblobi(Bti8, tt);
	lappend(&sub, &nsub, bt);
	switch (ty->type) {
	case Ntypes: case Tyvar: case Tybad: case Typaram:
	case Tygeneric: case Tycode: case Tyunres:
		die("invalid type in tydesc");
		break;

		/* atomic types -- nothing else to do */
	case Tyvoid: case Tychar: case Tybool: case Tyint8:
	case Tyint16: case Tyint: case Tyint32: case Tyint64:
	case Tybyte: case Tyuint8: case Tyuint16:
	case Tyuint: case Tyuint32: case Tyuint64:
	case Tyflt32: case Tyflt64: case Tyvalist:
		break;

	case Typtr:
		lappend(&sub, &nsub, tydescsub(ty->sub[0]));
		break;
	case Tyslice:
		lappend(&sub, &nsub, tydescsub(ty->sub[0]));
		break;
	case Tyarray:
		encodetypeinfo(&sub, &nsub, ty);
		ty->asize = fold(ty->asize, 1);
		len = ty->asize;
		if (len) {
			assert(len->type == Nexpr);
			len = len->expr.args[0];
			assert(len->type == Nlit && len->lit.littype == Lint);
			lappend(&sub, &nsub, mkblobi(Btimin, len->lit.intval));
		} else {
			lappend(&sub, &nsub, mkblobi(Btimin, 0));
		}

		lappend(&sub, &nsub, tydescsub(ty->sub[0]));
		break;
	case Tyfunc:
		lappend(&sub, &nsub, mkblobi(Btimin, ty->nsub));
		for (i = 0; i < ty->nsub; i++)
			lappend(&sub, &nsub, tydescsub(ty->sub[i]));
		break;
	case Tytuple:
		encodetypeinfo(&sub, &nsub, ty);
		lappend(&sub, &nsub, mkblobi(Btimin, ty->nsub));
		for (i = 0; i < ty->nsub; i++)
			lappend(&sub, &nsub, tydescsub(ty->sub[i]));
		break;
	case Tystruct:
		encodetypeinfo(&sub, &nsub, ty);
		lappend(&sub, &nsub, mkblobi(Btimin, ty->nmemb));
		for (i = 0; i < ty->nmemb; i++)
			structmemb(&sub, &nsub, ty->sdecls[i]);
		break;
	case Tyunion:
		encodetypeinfo(&sub, &nsub, ty);
		lappend(&sub, &nsub, mkblobi(Btimin, ty->nmemb));
		for (i = 0; i < ty->nmemb; i++)
			unionmemb(&sub, &nsub, ty->udecls[i]);
		break;
	case Tyname:
		i = bprintf(buf, sizeof buf, "%s", Symprefix);
		tydescid(buf + i, sizeof buf - i, ty);
		isextern = ty->isimport || ty->vis != Visintern;
		lappend(&sub, &nsub, mkblobref(buf, 0, isextern));
		break;
	}
	b = mkblobseq(sub, nsub);
	sz->ival = blobsz(b);
	linsert(&b->seq.sub, &b->seq.nsub, 0, sz);
	return b;
}

Blob *namedesc(Type *ty)
{
	Blob **sub;
	size_t nsub;

	sub = NULL;
	nsub = 0;
	lappend(&sub, &nsub, mkblobi(Bti8, Tyname));
	namevec(&sub, &nsub, ty->name);
	lappend(&sub, &nsub, tydescsub(ty->sub[0]));
	return mkblobseq(sub, nsub);
}

Blob *tydescblob(Type *ty)
{
	char buf[512];
	Blob *b, *sz, *sub;

	if (ty->type == Tyname && hasparams(ty))
		return NULL;

	if (ty->type == Tyname) {
		b = mkblobseq(NULL, 0);
		sz = mkblobi(Btimin, 0);
		sub = namedesc(ty);
		sz->ival = blobsz(sub);
		lappend(&b->seq.sub, &b->seq.nsub, sz);
		lappend(&b->seq.sub, &b->seq.nsub, sub);
		if (ty->vis != Visintern)
			b->isglobl = 1;
	} else {
		b = tydescsub(ty);
	}
	tydescid(buf, sizeof buf, ty);
	b->lbl = strdup(buf);
	return b;
}

size_t tysize(Type *t)
{
	size_t sz;
	size_t i;

	sz = 0;
	if (!t)
		die("size of empty type => bailing.");
	switch (t->type) {
	case Tyvoid:
		return 0;
	case Tybool: case Tyint8:
	case Tybyte: case Tyuint8:
		return 1;
	case Tyint16: case Tyuint16:
		return 2;
	case Tyint: case Tyint32:
	case Tyuint: case Tyuint32:
	case Tychar:  /* utf32 */
		return 4;

	case Typtr:
	case Tyvalist: /* ptr to first element of valist */
		return Ptrsz;

	case Tyint64:
	case Tyuint64:
		return 8;

		/*end integer types*/
	case Tyflt32:
		return 4;
	case Tyflt64:
		return 8;

	case Tycode:
		return Ptrsz;
	case Tyfunc:
		return 2*Ptrsz;
	case Tyslice:
		return 2*Ptrsz; /* len; ptr */
	case Tyname:
		return tysize(t->sub[0]);
	case Tyarray:
		if (!t->asize)
			return 0;
		t->asize = fold(t->asize, 1);
		assert(exprop(t->asize) == Olit);
		return t->asize->expr.args[0]->lit.intval * tysize(t->sub[0]);
	case Tytuple:
		for (i = 0; i < t->nsub; i++) {
			sz = alignto(sz, t->sub[i]);
			sz += tysize(t->sub[i]);
		}
		sz = alignto(sz, t);
		return sz;
		break;
	case Tystruct:
		for (i = 0; i < t->nmemb; i++) {
			sz = alignto(sz, decltype(t->sdecls[i]));
			sz += size(t->sdecls[i]);
		}
		sz = alignto(sz, t);
		return sz;
		break;
	case Tyunion:
		sz = Wordsz;
		for (i = 0; i < t->nmemb; i++)
			if (t->udecls[i]->etype)
				sz = max(sz, tysize(t->udecls[i]->etype) + Wordsz);
		return align(sz, tyalign(t));
		break;
	case Tygeneric: case Tybad: case Tyvar:
	case Typaram: case Tyunres: case Ntypes:
		die("Type %s does not have size; why did it get down to here?", tystr(t));
		break;
	}
	return -1;
}

size_t tyalign(Type *ty)
{
	size_t align, i;

	align = 1;
	ty = tybase(ty);
	switch (ty->type) {
	case Tyarray:
		align = tyalign(ty->sub[0]);
		break;
	case Tytuple:
		for (i = 0; i < ty->nsub; i++)
			align = max(align, tyalign(ty->sub[0]));
		break;
	case Tyunion:
		align = 4;
		for (i = 0; i < ty->nmemb; i++)
			if (ty->udecls[i]->etype)
				align = max(align, tyalign(ty->udecls[i]->etype));
		break;
	case Tystruct:
		for (i = 0; i < ty->nmemb; i++)
			align = max(align, tyalign(decltype(ty->sdecls[i])));
		break;
	default:
		align = max(align, tysize(ty));
	}
	return min(align, Ptrsz);
}

/* gets the byte offset of 'memb' within the aggregate type 'aggr' */
ssize_t tyoffset(Type *ty, Node *memb)
{
	size_t i;
	size_t off;

	ty = tybase(ty);
	if (ty->type == Typtr)
		ty = tybase(ty->sub[0]);

	assert(ty->type == Tystruct);
	off = 0;
	for (i = 0; i < ty->nmemb; i++) {
		off = alignto(off, decltype(ty->sdecls[i]));
		if (!strcmp(namestr(memb), declname(ty->sdecls[i])))
			return off;
		off += size(ty->sdecls[i]);
	}
	die("bad offset");
	return 0;
}

size_t size(Node *n)
{
	Type *t;

	if (n->type == Nexpr)
		t = n->expr.type;
	else
		t = n->decl.type;
	return tysize(t);
}

ssize_t offset(Node *aggr, Node *memb)
{
	return tyoffset(exprtype(aggr), memb);
}

char *tydescid(char *buf, size_t bufsz, Type *ty)
{
	char *sep, *ns;
	char *p, *end;
	size_t i;

	sep = "";
	ns = "";
	p = buf;
	end = buf + bufsz;
	ty = tydedup(ty);
	if (ty->type == Tyname) {
		if (ty->name->name.ns) {
			ns = ty->name->name.ns;
			sep = "$";
		}
		if (ty->vis != Visintern || ty->isimport)
			p += bprintf(p, end - p, "_tydesc$%s%s%s", ns, sep, ty->name->name.name, ty->tid);
		else
			p += bprintf(p, end - p, "_tydesc$%s%s%s$%d", ns, sep, ty->name->name.name, ty->tid);
		for (i = 0; i < ty->narg; i++)
			p += tyidfmt(p, end - p, ty->arg[i]);
	} else {
		if (file->file.globls->name) {
			ns = file->file.globls->name;
			sep = "$";
		}
		bprintf(buf, bufsz, "_tydesc%s%s$%d",sep, ns, ty->tid);
	}
	return buf;
}

