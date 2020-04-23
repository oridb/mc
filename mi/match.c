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

Dtree *gendtree(Node *m, Node *val, Node **lbl, size_t nlbl);
void dtreedump(FILE *fd, Dtree *dt);


static size_t ndtree;
static Dtree **dtree;

/* Path is a integer sequence that labels a subtree of a subject tree.
 * For example,
 * 0 is the root
 * 0,0 and 0,1 are two subtrees of the root.
 *
 * Note: the order of the sequence is reversed with regard to the reference paper
 * "When Do Match-Compilation Heuristics Matter" by Kevin Scott and Norman Ramse.
 *
 * Each pattern of a match clause conrresponds to a unique Path of the subject tree.
 * When we have m match clauses, for a given Path, there can be at most m patterns
 * associated with the Path.
 *
 * Given
 * match v
 * | (11, 12, 13)
 * | (21, 22, 23)
 * | (31, 32, 33)
 * | _
 *
 * the entries 11, 21, 31 and the wildcard pattern at the bottom have the same Path 0,1
 *             12, 22, 32 have the same Path 0,2
 *             13, 23, 33 have the same Path 0,3
 */
typedef struct Path {
	size_t len;
	int *p;
} Path;

/* Slot bundles a pattern with its corresponding Path and load value.
 * The compiler will generate a comparison instruction for each (pat, load) pair.
 * Each match clause corresponds to a list of slots which is populated by addrec.
 */
typedef struct Slot {
	Path *path;
	Node *pat;
	Node *load;
} Slot;

static Slot *
mkslot(Path *path, Node *pat, Node *val)
{
	Slot *s;
	s = zalloc(sizeof(Slot));
	s->path = path;
	s->pat = pat;
	s->load = val;
	assert(path != (void *)1);
	return s;
}

/* Frontier bundles a list of slots to be matched for a specific match clause.
 * The instances of Frontier should be immutable after creation.
 */
typedef struct Frontier {
	int i; 			/* index of the match clauses from top to bottom */
	Node *lbl; 		/* branch target when the rule is matched */
	Slot  **slot; 		/* unmatched slots (Paths) for this match clause */
	size_t nslot;
	Node **cap; 		/* the captured variables of the pattern of this match clause */
	size_t ncap;
	Dtree *final;		/* final state, shared by all Frontiers for a specific (indxed by i) match clause */

	struct Frontier *next;
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
	t->id = ndtree;
	lappend(&dtree, &ndtree, t);
	return t;
}

static int
loadeq(Node *a, Node *b)
{
	if (a == b)
		return 1;

	if (exprop(a) != exprop(b))
		return 0;

	switch (exprop(a)) {
	case Outag:
	case Oudata:
	case Oderef:
		return loadeq(a->expr.args[0], b->expr.args[0]);
	case Omemb:
		return loadeq(a->expr.args[0], b->expr.args[0]) && nameeq(a->expr.args[1], b->expr.args[1]);
	case Otupget:
	case Oidx:
		return loadeq(a->expr.args[0], b->expr.args[0]) && liteq(a->expr.args[1]->expr.args[0], b->expr.args[1]->expr.args[0]);
	case Ovar:
		return a == b;
	default:
		die("unreachable");
	}
	return 0;
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
		die("unreachable");
	}
	return 0;
}

static int
dtreusable(Dtree *dt, Node *load, Node **pat, size_t npat, Dtree **next, size_t nnext, Dtree *any)
{
	size_t i;

	if (!loadeq(dt->load, load))
		return 0;

	if (dt->npat != npat)
		return 0;

	for (i = 0; i < npat; i++) {
		if (dt->next[i]->id != next[i]->id)
			return 0;
		if (!pateq(dt->pat[i], pat[i]))
			return 0;
	}

	if (dt->any == NULL && any == NULL)
		return 1;

	if (dt->any->id != any->id)
		return 0;

	return 1;
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
mkpath(Path *p, int i)
{
	Path *newp;

	newp = zalloc(sizeof(Path));
	if (p) {
		newp->p = zalloc(sizeof(newp->p[0])*(p->len+1));
		memcpy(newp->p, p->p, sizeof(newp->p[0])*p->len);
		newp->p[p->len] = i;
		newp->len = p->len+1;
	} else {
		newp->p = zalloc(sizeof(int)*4);
		newp->p[0] = 0;
		newp->len = 1;
	}
	assert(newp->p != 0);
	return newp;
}

static int
patheq(Path *a, Path *b)
{
	size_t i;

	if (a->len != b->len)
		return 0;

	for (i = 0; i < a->len; i++) {
		if (a->p[i] != b->p[i])
			return 0;
	}
	return 1;
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

static size_t
nconstructors(Type *t)
{
	if (!t)
		return 0;

	t = tybase(t);
	switch (t->type) {
	case Tyvoid:	return 1;
	case Tybool:	return 2;
	case Tychar:	return 0x10ffff;

	/* signed ints */
	case Tyint8:	return 0x100;
	case Tyint16:	return 0x10000;
	case Tyint32:	return 0x100000000;
	case Tyint:	return 0x100000000;
	case Tyint64:	return ~0ull;

	/* unsigned ints */
	case Tybyte:	return 0x100;
	case Tyuint8:	return 0x100;
	case Tyuint16:	return 0x10000;
	case Tyuint32:	return 0x100000000;
	case Tyuint:	return 0x100000000;
	case Tyuint64:	return ~0ull;

	/* floats */
	case Tyflt32:	return ~0ull;
	case Tyflt64:	return ~0ull;

	/* complex types */
	case Typtr:	return 1;
	case Tyarray:	return 1;
	case Tytuple:	return 1;
	case Tystruct:	return 1;
	case Tyunion:	return t->nmemb;
	case Tyslice:	return ~0ULL;

	case Tyvar: case Typaram: case Tyunres: case Tyname:
	case Tybad: case Tyvalist: case Tygeneric: case Ntypes:
	case Tyfunc: case Tycode:
		     die("Invalid constructor type %s in match", tystr(t));
		     break;

	}
	return 0;
}

static Frontier *
mkfrontier(int i, Node *lbl)
{
	Frontier *fs;

	fs = zalloc(sizeof(Frontier));
	fs->i = i;
	fs->lbl = lbl;
	fs->final = mkdtree(lbl->loc, lbl);
	fs->final->accept = 1;
	return fs;;
}

/*
 * All immutable fields are shared; mutable fields must be cloned *
 */
static Frontier *
frontierdup(Frontier *fs)
{
	Frontier *out;

	out = mkfrontier(fs->i, fs->lbl);
	out->slot = memdup(fs->slot, fs->nslot*sizeof(fs->slot[0]));
	out->nslot = fs->nslot;
	out->cap = memdup(fs->cap, fs->ncap*sizeof(fs->cap[0]));
	out->ncap = fs->ncap;
	return out;
}


/* addrec generates a list of slots for a Frontier by walking a pattern tree.
 * It collects only the terminal patterns like union tags and literals.
 * Non-terminal patterns like tuple/struct/array help encode the path only.
 */
static void
addrec(Frontier *fs, Node *pat, Node *val, Path *path)
{
	size_t i, n;
	Type *ty, *mty;
	Node *memb, *name, *tagid, *p, *v, *lit, *dcl, *asn, *deref;
	Ucon *uc;
	char *s;
	Frontier *next;

	pat = fold(pat, 1);
	switch (exprop(pat)) {
	case Olor:
		next = frontierdup(fs);
		next->next = fs->next;
		fs->next = next;
		addrec(fs, pat->expr.args[1], val, path);
		addrec(next, pat->expr.args[0], val, path);
		break;
	case Ogap:
		lappend(&fs->slot, &fs->nslot, mkslot(path, pat, val));
		break;
	case Ovar:
		dcl = decls[pat->expr.did];
		if (dcl->decl.isconst) {
			ty = decltype(dcl);
			if (ty->type == Tyfunc || ty->type == Tycode || ty->type == Tyvalist)
				fatal(dcl, "bad pattern %s:%s: unmatchable type", declname(dcl), tystr(ty));
			if (!dcl->decl.init)
				fatal(dcl, "bad pattern %s:%s: missing initializer", declname(dcl), tystr(ty));
			addrec(fs, dcl->decl.init, val, mkpath(path, 0));
		} else {
			asn = mkexpr(pat->loc, Oasn, pat, val, NULL);
			asn->expr.type = exprtype(pat);
			lappend(&fs->cap, &fs->ncap, asn);

			lappend(&fs->slot, &fs->nslot, mkslot(path, pat, val));
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

			addrec(fs, p, v, mkpath(path, 0));

			ty = mktype(pat->loc, Tybyte);
			for (i = 0; i < n; i++) {
				p = mkintlit(lit->loc, s[i]);
				p->expr.type = ty;
				v = arrayelt(val, i);
				addrec(fs, p, v, mkpath(path, 1+i));
			}
		} else {
			lappend(&fs->slot, &fs->nslot, mkslot(path, pat, val));
		}
		break;
	case Oaddr:
		deref = mkexpr(val->loc, Oderef, val, NULL);
		deref->expr.type = exprtype(pat->expr.args[0]);
		addrec(fs, pat->expr.args[0], deref, mkpath(path, 0));
		break;
	case Oucon:
		uc = finducon(tybase(exprtype(pat)), pat->expr.args[0]);
		tagid = mkintlit(pat->loc, uc->id);
		tagid->expr.type = mktype(pat->loc, Tyint32);
		addrec(fs, tagid, utag(val), mkpath(path, 0));
		if (uc->etype)
			addrec(fs, pat->expr.args[1], uvalue(val, uc->etype), mkpath(path, 1));
		break;
	case Otup:
		for (i = 0; i < pat->expr.nargs; i++) {
			addrec(fs, pat->expr.args[i], tupelt(val, i), mkpath(path, i));
		}
		break;
	case Oarr:
		for (i = 0; i < pat->expr.nargs; i++) {
			addrec(fs, pat->expr.args[i], arrayelt(val, i), mkpath(path, i));
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
			addrec(fs, memb, structmemb(val, name, mty), mkpath(path, i));
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

	fs = mkfrontier(i, lbl);
	addrec(fs, pat, val, mkpath(NULL, 0));
	while (fs != NULL) {
		lappend(frontier, nfrontier, fs);
		fs = fs->next;
	}
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
	out->final = fs->final;

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

/* compile implements the algorithm outlined in the paper
 * "When Do Match-Compilation Heuristics Matter?" by Kevin Scott and Norman Ramsey
 * It generates either a TEST or MATCH (accept=1) dtree, where MATCH is the base case.
 *
 * Summary:
 * 1. if the first Frontier of the input list of Frontiers does not contain any
 *    non-wildcard pattern, return a MATCH dtree (accept=1)
 * 2. for each call to the function, it will select a Path from
 *    the first match clause in the input Frontiers.
 * 3. scan the input frontiers 'vertically' at the selected Path to form a set
 *    of unique constructors. (the list 'cs')
 * 4. for each constructor in 'cs', recursively compile the 'projected' frontiers,
 *    which is roughly the frontiers minus the slot at the Path.
 * 5. recursively compile the remaining Frontiers (if any) corresponding to the matches
 *    rules with a wildcard at the selected Path.
 * 6. return a new dtree, where the compiled outputs at step 4 form the outgoing edges
 *    (dt->next), and the one produced at step 5 serves as the default edage (dt->any)
 *
 * NOTE:
 * a. how we select the Path at the step 2 is determined by heuristics.
 * b. we don't expand the frontiers at the 'project' step as the reference paper does.
 *    rather, we separate the whole compile algorithm into two phases:
 *    1) construction of the initial frontiers by 'addrec'.
 *    2) construction of the decision tree (with the generated frontiers) by 'compile'
 */
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
		fs->final->refcnt++;
		return fs->final;
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
	out = NULL;

	/* TODO
	 * use a hash table to avoid the quadratic complexity
	 * when we have a large N and the bottleneck becomes obvious.
	 */
	for (i = 0; i < ndtree; i++) {
		if (!dtree[i]->accept && dtreusable(dtree[i], slot->load, pat, npat, edge, nedge, any)) {
			out = dtree[i];
			out->refcnt++;
			break;
		}
	}
	if (out == NULL) {
		out = mkdtree(slot->pat->loc, genlbl(slot->pat->loc));
		out->refcnt++;
	}

	out->load = slot->load;
	out->npat = npat,
	out->pat = pat,
	out->nnext = nedge;
	out->next = edge;
	out->any = any;

	switch (exprop(slot->load)) {
	case Outag:
		out->nconstructors = nconstructors(tybase(exprtype(slot->load->expr.args[0])));
		break;
	case Oudata:
	case Omemb:
	case Otupget:
	case Oidx:
	case Oderef:
	case Ovar:
		out->nconstructors = nconstructors(tybase(exprtype(slot->load)));
		break;
	default:
		fatal(slot->pat, "unsupported pattern %s of type %s", opstr[exprop(slot->pat)], tystr(exprtype(slot->pat)));
	}
	return out;
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

static size_t
dtheight(Dtree *dt)
{
	size_t i, h, m;

	if (dt == NULL)
		return 0;
	if (dt->accept)
		return 0;

	m = 0;
	for (i = 0; i < dt->nnext; i++) {
		h = dtheight(dt->next[i]);
		if (h > m)
			m = h;
	}
	h = dtheight(dt->any);
	if (h > m)
		m = h;
	return m+1;
}

static size_t
refsum(Dtree *dt)
{
	size_t i;
	size_t sum;

	if (dt == NULL)
		return 0;

	dt->emitted = 1;

	/* NOTE
	 * MATCH nodes are always pre-allocated and shared,
	 * so counted as 1 for the size measurement.
	 */
	if (dt->accept)
		return 1;

	sum = 0;
	for (i = 0; i < dt->nnext; i++)
		if (!dt->next[i]->emitted)
			sum += refsum(dt->next[i]);
	if (dt->any && !dt->any->emitted)
		sum += refsum(dt->any);

	return dt->refcnt + sum;
}

static void
clearemit(Dtree *dt)
{
	size_t i;

	if (dt == NULL)
		return;
	for (i = 0; i < dt->nnext; i++)
		clearemit(dt->next[i]);
	clearemit(dt->any);
	dt->emitted = 0;
}

static int
capeq(Node *a, Node *b)
{
	Node *pa, *pb, *va, *vb;

	pa = a->expr.args[0];
	pb = b->expr.args[0];
	va = a->expr.args[1];
	vb = b->expr.args[1];

	return pa->expr.did == pb->expr.did && loadeq(va, vb);
}

Dtree *
gendtree(Node *m, Node *val, Node **lbl, size_t nlbl)
{
	Dtree *root;
	Node **pat;
	size_t npat;
	size_t i, j;
	Frontier **frontier, *cur, *last;
	size_t nfrontier;
	FILE *csv;
	char *dbgloc, *dbgfn, *dbgln;


	ndtree = 0;
	dtree = NULL;

	pat = m->matchstmt.matches;
	npat = m->matchstmt.nmatches;

	frontier = NULL;
	nfrontier = 0;
	for (i = 0; i < npat; i++) {
		genfrontier(i, val, pat[i]->match.pat, lbl[i], &frontier, &nfrontier);
	}

	/* to determine if two different sets of captures come from a or-pattern, which is NOT allowed. */
	last = NULL;
	for (i = 0; i < nfrontier; i++) {
		cur = frontier[i];
		if (last && last->i == cur->i) {
			if (last->ncap != cur->ncap)
				fatal(pat[cur->i], "number of wildcard variables in the or-patterns are not equal (%d != %d)", last->ncap, cur->ncap);
			for (j = 0; j < cur->ncap; j++) {
				if (!capeq(last->cap[j], cur->cap[j]))
					fatal(pat[cur->i], "wildcard variables have different types in the or-patterns");
			}
		} else {
			addcapture(pat[cur->i]->match.block, cur->cap, cur->ncap);
		}
		last = cur;
	}
	root = compile(frontier, nfrontier);

	for (i = 0; i < nfrontier; i++)
		if (frontier[i]->final->refcnt == 0)
			fatal(pat[i], "pattern matched by earlier case");

	if (debugopt['M'] || getenv("M")) {
		dbgloc = strdup(getenv("M"));
		if (strchr(dbgloc, '@')) {
			dbgfn = strtok(dbgloc, "@");
			dbgln = strtok(NULL, "@")+1; /* offset by 1 to skip the charactor 'L' */
			if (!strcmp(fname(m->loc), dbgfn) && lnum(m->loc) == strtol(dbgln, 0, 0))
				dtreedump(stdout, root);
		}
		else
			dtreedump(stdout, root);

	}
	if (!verifymatch(root)) {
		fatal(m, "nonexhaustive pattern set in match statement");
	}
	if (getenv("MATCH_STATS")) {
		csv = fopen("match.csv", "a");
		assert(csv != NULL);
		fprintf(csv, "%s@L%d, %ld, %zd, %zd\n", fname(m->loc), lnum(m->loc), refsum(root), ndtree, dtheight(root));
		fclose(csv);

		/* clear 'emitted' so it can be reused by genmatchcode. */
		clearemit(root);
	}

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

	ndtree = 0;
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
	dt = gendtree(m, val, lbl, nlbl);
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
