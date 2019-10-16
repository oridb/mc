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

Dtree *gendtree(Node *m, Node *val, Node **lbl, size_t nlbl, int startid);
void dtreedump(FILE *fd, Dtree *dt);


static int ndtree;

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

/*
 * The instances of Frontier should be immutable after creation.
 */
typedef struct Frontier {
	int i;
	Node *lbl;
	Slot  **slot;
	size_t nslot;
	Node **cap;
	size_t ncap;
} Frontier;

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

	if (a->len != b->len)
		return 0;

	for (i = 0; i < a->len; i++) {
		if (a->p[i] != b->p[i])
			return 0;
	}
	return 1;
}

static int
pateq(Node *a, Node *b)
{
	if (exprop(a) != exprop(b))
		return 0;

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
			if (ty->type == Tyfunc || ty->type == Tycode || ty->type == Tyvalist)
				fatal(dcl, "bad pattern %s:%s: unmatchable type", declname(dcl), tystr(ty));
			if (!dcl->decl.init)
				fatal(dcl, "bad pattern %s:%s: missing initializer", declname(dcl), tystr(ty));
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
		if (uc->etype)
			addrec(fs, pat->expr.args[1], uvalue(val, uc->etype), newpath(path, 1));
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

/*
 * project generates a new frontier with a new the set of slots by reducing the input slots.
 * literally, it deletes the slot at the path pi.
 */
static Frontier *
project(Node *pat, Path *pi, Node *val, Frontier *fs)
{
	Frontier *out;
	Slot *c, **slot;
	size_t i, nslot;

	assert (fs->nslot > 0);

	/*
	 * copy a new set of slots from fs without the slot at the path pi.
	 * c points to the slot at the path pi.
	 */
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

	out = zalloc(sizeof(Frontier));
	out->i = fs->i;
	out->lbl = fs->lbl;
	out->slot = slot;
	out->nslot = nslot;
	out->cap = fs->cap;
	out->ncap = fs->ncap;

	/*
	 * if the sub-term at pi is not in the frontier,
	 * then we do not reduce the frontier.
	 */
	if (c == NULL)
		return out;

	switch (exprop(c->pat)) {
	case Ovar:
	case Ogap:
		/*
		 * if the pattern at the sub-term pi of this frontier is not a constructor,
		 * then we do not reduce the frontier.
		 */
		return out;
	default:
		break;
	}

	/*
	 * if constructor at the path pi is not the constructor we want to project,
	 * then return null.
	 */
	if (!pateq(pat, c->pat))
		return NULL;

	return out;
}

static Dtree *
compile(Frontier **frontier, size_t nfrontier)
{
	size_t i, j, k;
	Dtree *dt, *out, **edge, *any;
	Frontier *fs, **row, **defaults ;
	Node **cs, **pat;
	Slot *slot, *s;
	size_t ncs, ncons, nrow, nedge, ndefaults, npat;

	assert(nfrontier > 0);

	fs = frontier[0];

	/* scan constructors horizontally */
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
		out = mkdtree(fs->lbl->loc, fs->lbl);
		out->accept = 1;
		return out;
	}

	assert(fs->nslot > 0);

	/* NOTE:
	 * at the moment we have not implemented any smarter heuristics described in the papers.
	 * we always select the first found constructor, i.e. the top-left one.
	 */

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
	/* scan constructors vertically at pi to create the set 'CS' */
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
					/* NOTE:
					 * we could use a hash table, but given that the n is usually small,
					 * an exhaustive search would suffice.
					 */
					/* look for a duplicate entry to skip it; we want a unique set of constructors. */
					for (k = 0; k < ncs; k++) {
						if (pateq(cs[k], s->pat))
							break;
					}
					if (ncs == 0 || k == ncs)
						lappend(&cs, &ncs, s->pat);
				}
				break;
			default:
				assert(0);
			}
		}
	}
	/* project a new frontier for each selected constructor */
	edge = NULL;
	nedge = 0;
	pat = NULL;
	npat = 0;
	for (i = 0; i < ncs; i++) {
		row = NULL;
		nrow = 0;
		for (j = 0; j < nfrontier; j++) {
			fs = frontier[j];
			fs = project(cs[i], slot->path, slot->load, frontier[j]);
			if (fs != NULL)
				lappend(&row, &nrow, fs);
		}

		if (nrow > 0) {
			dt = compile(row, nrow);
			lappend(&edge, &nedge, dt);
			lappend(&pat, &npat, cs[i]);
		}
	}

	/* compile the defaults */
	defaults = NULL;
	ndefaults = 0;
	for (i = 0; i < nfrontier; i++) {
		fs = frontier[i];
		k = -1;
		for (j = 0; j < fs->nslot; j++) {
			/* locate the occurrence of pi in fs */
			if (patheq(slot->path, fs->slot[j]->path))
				k = j;
		}

		if (k == -1 || exprop(fs->slot[k]->pat) == Ovar || exprop(fs->slot[k]->pat) == Ogap)
			lappend(&defaults, &ndefaults, fs);
	}
	if (ndefaults)
		any = compile(defaults, ndefaults);
	else
		any = NULL;

	/* construct the result dtree */
	out = mkdtree(slot->pat->loc, genlbl(slot->pat->loc));
	out->load = slot->load;
	out->npat = npat,
	out->pat = pat,
	out->nnext = nedge;
	out->next = edge;
	out->any = any;

	return out;
}


Dtree *
gendtree(Node *m, Node *val, Node **lbl, size_t nlbl, int startid)
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

	if (debugopt['M'] || getenv("M"))
		dtreedump(stdout, root);

	return root;
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
	Frontier **frontier;
	size_t nfrontier;
	Dtree *root;

	frontier = NULL;
	nfrontier = 0;
	genfrontier(0, val, pat, iftrue, &frontier, &nfrontier);
	lcat(cap, ncap, frontier[0]->cap, frontier[0]->ncap);
	genfrontier(1, val, mkexpr(iffalse->loc, Ogap, NULL), iffalse, &frontier, &nfrontier);
	root = compile(frontier, nfrontier);
	genmatchcode(root, out, nout);
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
		dt = gendtree(m, val, lbl, nlbl, ndtree);
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
