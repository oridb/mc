#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "util.h"
#include "parse.h"
#include "mi.h"
#include "asm.h"

#define Sizetbits (CHAR_BIT*sizeof(size_t)) /* used in graph reprs */

typedef struct Usemap Usemap;
struct Usemap {
	int l[Nreg + 1]; /* location of arg used in instruction's arg list */
	int r[Nreg + 1]; /* list of registers used implicitly by instruction */
};

void wlprint(FILE *fd, char *name, Loc **wl, size_t nwl);
static int moverelated(Isel *s, regid n);
static void printedge(FILE *fd, char *msg, size_t a, size_t b);

/* tables of uses/defs by instruction */
Usemap usetab[] = {
#define Def(...)
#define Use(...) {__VA_ARGS__}
#define Insn(i, gasfmt, p9fmt, use, def) use,
#include "insns.def"
#undef Insn
#undef Use
#undef Def
};

Usemap deftab[] = {
#define Use(...)
#define Def(...) {__VA_ARGS__}
#define Insn(i, gasfmt, p9fmt, use, def) def,
#include "insns.def"
#undef Insn
#undef Def
#undef Use
};

/*
 * A map of which registers interfere. A write to
 * any entry in regmap[colormap[reg]] will clobber
 * any of the other values in that row of regmap.
 */
#define Northogonal (32 - 2)
Reg regmap[Northogonal][Nmode] = {
		/* None,   ModeB, ModeW, ModeL, ModeQ, ModeF, ModeD */
	[0]	= {Rnone, Ral,   Rax,   Reax,  Rrax,  Rnone,  Rnone},
	[1]	= {Rnone, Rcl,   Rcx,   Recx,  Rrcx,  Rnone,  Rnone},
	[2]	= {Rnone, Rdl,   Rdx,   Redx,  Rrdx,  Rnone,  Rnone},
	[3]	= {Rnone, Rbl,   Rbx,   Rebx,  Rrbx,  Rnone,  Rnone},
	[4]	= {Rnone, Rsil,  Rsi,   Resi,  Rrsi,  Rnone,  Rnone},
	[5]	= {Rnone, Rdil,  Rdi,   Redi,  Rrdi,  Rnone,  Rnone},
	[6]	= {Rnone, Rr8b,  Rr8w,  Rr8d,  Rr8,   Rnone,  Rnone},
	[7]	= {Rnone, Rr9b,  Rr9w,  Rr9d,  Rr9,   Rnone,  Rnone},
	[8]	= {Rnone, Rr10b, Rr10w, Rr10d, Rr10,  Rnone,  Rnone},
	[9]	= {Rnone, Rr11b, Rr11w, Rr11d, Rr11,  Rnone,  Rnone},
	[10]	= {Rnone, Rr12b, Rr12w, Rr12d, Rr12,  Rnone,  Rnone},
	[11]	= {Rnone, Rr13b, Rr13w, Rr13d, Rr13,  Rnone,  Rnone},
	[12]	= {Rnone, Rr14b, Rr14w, Rr14d, Rr14,  Rnone,  Rnone},
	[13]	= {Rnone, Rr15b, Rr15w, Rr15d, Rr15,  Rnone,  Rnone},
	[14]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm0f, Rxmm0d},
	[15]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm1f, Rxmm1d},
	[16]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm2f, Rxmm2d},
	[17]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm3f, Rxmm3d},
	[18]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm4f, Rxmm4d},
	[19]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm5f, Rxmm5d},
	[20]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm6f, Rxmm6d},
	[21]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm7f, Rxmm7d},
	[22]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm8f, Rxmm8d},
	[23]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm9f, Rxmm9d},
	[24]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm10f, Rxmm10d},
	[25]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm11f, Rxmm11d},
	[26]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm12f, Rxmm12d},
	[27]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm13f, Rxmm13d},
	[28]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm14f, Rxmm14d},
	[29]	= {Rnone, Rnone, Rnone, Rnone, Rnone, Rxmm15f, Rxmm15d},
};

/* Which regmap entry a register maps to */
int colourmap[Nreg] = {
	/* byte */
	[Ral]	= 0,  [Rax]	= 0,  [Reax]	= 0,  [Rrax]	= 0,
	[Rcl]	= 1,  [Rcx]	= 1,  [Recx]	= 1,  [Rrcx]	= 1,
	[Rdl]	= 2,  [Rdx]	= 2,  [Redx]	= 2,  [Rrdx]	= 2,
	[Rbl]	= 3,  [Rbx]	= 3,  [Rebx]	= 3,  [Rrbx]	= 3,
	[Rsil]	= 4,  [Rsi]	= 4,  [Resi]	= 4,  [Rrsi]	= 4,
	[Rdil]	= 5,  [Rdi]	= 5,  [Redi]	= 5,  [Rrdi]	= 5,
	[Rr8b]	= 6,  [Rr8w]	= 6,  [Rr8d]	= 6,  [Rr8]	= 6,
	[Rr9b]	= 7,  [Rr9w]	= 7,  [Rr9d]	= 7,  [Rr9]	= 7,
	[Rr10b]	= 8,  [Rr10w]	= 8,  [Rr10d]	= 8,  [Rr10]	= 8,
	[Rr11b]	= 9,  [Rr11w]	= 9,  [Rr11d]	= 9,  [Rr11]	= 9,
	[Rr12b]	= 10, [Rr12w]	= 10, [Rr12d]	= 10, [Rr12]	= 10,
	[Rr13b]	= 11, [Rr13w]	= 11, [Rr13d]	= 11, [Rr13]	= 11,
	[Rr14b]	= 12, [Rr14w]	= 12, [Rr14d]	= 12, [Rr14]	= 12,
	[Rr15b]	= 13, [Rr15w]	= 13, [Rr15d]	= 13, [Rr15]	= 13,
	[Rrsp]	= 14, [Rrbp]	= 15,

	/* float */
	[Rxmm0f]	= 14,  [Rxmm0d]		= 14,
	[Rxmm1f]	= 15,  [Rxmm1d]		= 15,
	[Rxmm2f]	= 16,  [Rxmm2d]		= 16,
	[Rxmm3f]	= 17,  [Rxmm3d]		= 17,
	[Rxmm4f]	= 18,  [Rxmm4d]		= 18,
	[Rxmm5f]	= 19,  [Rxmm5d]		= 19,
	[Rxmm6f]	= 20,  [Rxmm6d]		= 20,
	[Rxmm7f]	= 21,  [Rxmm7d]		= 21,
	[Rxmm8f]	= 22,  [Rxmm8d]		= 22,
	[Rxmm9f]	= 23,  [Rxmm9d]		= 23,
	[Rxmm10f]	= 24,  [Rxmm10d]	= 24,
	[Rxmm11f]	= 25,  [Rxmm11d]	= 25,
	[Rxmm12f]	= 26,  [Rxmm12d]	= 26,
	[Rxmm13f]	= 27,  [Rxmm13d]	= 27,
	[Rxmm14f]	= 28,  [Rxmm14d]	= 28,
	[Rxmm15f]	= 29,  [Rxmm15d]	= 29,
};

size_t modesize[Nmode] = {
	[ModeNone]	= 0,
	[ModeB]	= 1,
	[ModeW]	= 2,
	[ModeL]	= 4,
	[ModeQ]	= 8,
	[ModeF]	= 4,
	[ModeD]	= 8,
};

static int _K[Nclass] = {
	[Classbad] = 0,
	[Classint] = 14,
	[Classflt] = 16,
};

Rclass
rclass(Loc *l)
{
	switch (l->mode) {
	case ModeNone:	return Classbad;
	case Nmode:	return Classbad;
	case ModeB:	return Classint;
	case ModeW:	return Classint;
	case ModeL:	return Classint;
	case ModeQ:	return Classint;

	case ModeF:	return Classflt;
	case ModeD:	return Classflt;
	}
	return Classbad;
}

/* %esp, %ebp are not in the allocatable pool */
static int
isfixreg(Loc *l)
{
	if (l->reg.colour == Resp)
		return 1;
	if (l->reg.colour == Rebp)
		return 1;
	return 0;
}

static size_t
uses(Insn *insn, regid *u)
{
	size_t i, j;
	int k;
	Loc *m;

	j = 0;
	/* Add all the registers used and defined. Duplicate
	 * in this list are fine, since they're being added to
	 * a set anyways */
	for (i = 0; i < Maxarg; i++) {
		if (!usetab[insn->op].l[i])
			break;
		k = usetab[insn->op].l[i] - 1;
		/* non-registers are handled later */
		if (insn->args[k]->type == Locreg)
			if (!isfixreg(insn->args[k]))
				u[j++] = insn->args[k]->reg.id;
	}
	/* some insns don't reflect their defs in the args.
	 * These are explictly listed in the insn description */
	for (i = 0; i < Nreg; i++) {
		if (!usetab[insn->op].r[i])
			break;
		/* not a leak; physical registers get memoized */
		u[j++] = locphysreg(usetab[insn->op].r[i])->reg.id;
	}
	/* If the registers are in an address calculation,
	 * they're used no matter what. */
	for (i = 0; i < insn->nargs; i++) {
		m = insn->args[i];
		if (m->type != Locmem && m->type != Locmeml)
			continue;
		if (m->mem.base)
			if (!isfixreg(m->mem.base))
				u[j++] = m->mem.base->reg.id;
		if (m->mem.idx)
			if (!isfixreg(m->mem.base))
				u[j++] = m->mem.idx->reg.id;
	}
	return j;
}

static size_t
defs(Insn *insn, regid *d)
{
	size_t i, j;
	int k;

	j = 0;
	/* Add all the registers dsed and defined. Duplicate
	 * in this list are fine, since they're being added to
	 * a set anyways */
	for (i = 0; i < Maxarg; i++) {
		if (!deftab[insn->op].l[i])
			break;
		k = deftab[insn->op].l[i] - 1;
		if (insn->args[k]->type == Locreg)
			if (!isfixreg(insn->args[k]))
				d[j++] = insn->args[k]->reg.id;
	}
	/* some insns don't reflect their defs in the args.
	 * These are explictly listed in the insn description */
	for (i = 0; i < Nreg; i++) {
		if (!deftab[insn->op].r[i])
			break;
		/* not a leak; physical registers get memoized */
		d[j++] = locphysreg(deftab[insn->op].r[i])->reg.id;
	}
	return j;
}

/* The uses and defs for an entire BB. */
static void
udcalc(Asmbb *bb)
{
	regid   u[Nreg], d[Nreg];
	size_t nu, nd;
	size_t i, j;

	bb->use = bsclear(bb->use);
	bb->def = bsclear(bb->def);
	for (i = 0; i < bb->ni; i++) {
		nu = uses(bb->il[i], u);
		nd = defs(bb->il[i], d);
		for (j = 0; j < nu; j++)
			if (!bshas(bb->def, u[j]))
				bsput(bb->use, u[j]);
		for (j = 0; j < nd; j++)
			bsput(bb->def, d[j]);
	}
}

static int
istrivial(Isel *s, regid r)
{
	return s->degree[r] < _K[rclass(locmap[r])];
}

static void
liveness(Isel *s)
{
	Bitset *old;
	Asmbb **bb;
	ssize_t nbb;
	ssize_t i;
	size_t j;
	int changed;

	bb = s->bb;
	nbb = s->nbb;
	for (i = 0; i < nbb; i++) {
		if (!bb[i])
			continue;
		udcalc(s->bb[i]);
		bb[i]->livein = bsclear(bb[i]->livein);
		bb[i]->liveout = bsclear(bb[i]->liveout);
	}

	changed = 1;
	while (changed) {
		changed = 0;
		old = NULL;
		for (i = nbb - 1; i >= 0; i--) {
			if (!bb[i])
				continue;
			old = bsdup(bb[i]->liveout);
			/* liveout[b] = U(s in succ) livein[s] */
			for (j = 0; bsiter(bb[i]->succ, &j); j++)
				bsunion(bb[i]->liveout, bb[j]->livein);
			/* livein[b] = use[b] U (out[b] \ def[b]) */
			bb[i]->livein = bsclear(bb[i]->livein);
			bsunion(bb[i]->livein, bb[i]->liveout);
			bsdiff(bb[i]->livein, bb[i]->def);
			bsunion(bb[i]->livein, bb[i]->use);
			if (!changed)
				changed = !bseq(old, bb[i]->liveout);
			bsfree(old);
		}
	}
}

/* we're only interested in register->register moves */
static int
ismove(Insn *i)
{
	if (i->op != Imov && i->op != Imovs)
		return 0;
	return i->args[0]->type == Locreg && i->args[1]->type == Locreg;
}

static int
gbhasedge(Isel *s, size_t u, size_t v)
{
	size_t i;
	i = (s->nreg * v) + u;
	return (s->gbits[i/Sizetbits] & (1ULL <<(i % Sizetbits))) != 0;
}

static void
gbputedge(Isel *s, size_t u, size_t v)
{
	size_t i, j;

	i = (s->nreg * u) + v;
	j = (s->nreg * v) + u;
	s->gbits[i/Sizetbits] |= 1ULL << (i % Sizetbits);
	s->gbits[j/Sizetbits] |= 1ULL << (j % Sizetbits);
	assert(gbhasedge(s, u, v) && gbhasedge(s, v, u));
}

static int
wlfind(Loc **wl, size_t nwl, regid v, size_t *idx)
{
	size_t i;

	for (i = 0; i < nwl; i++) {
		if (wl[i]->reg.id == v) {
			*idx = i;
			return 1;
		}
	}
	*idx = -1;
	return 0;
}

/*
 * If we have an edge between two aliasing registers,
 * we should not increment the degree, since that would
 * be double counting.
 */
static int
degreechange(Isel *s, regid u, regid v)
{
	regid phys, virt, r;
	size_t i;

	if (bshas(s->prepainted, u)) {
		phys = u;
		virt = v;
	} else if (bshas(s->prepainted, v)) {
		phys = v;
		virt = u;
	} else {
		return 1;
	}

	for (i = 0; i < Nmode; i++) {
		r = regmap[colourmap[phys]][i];
		if (r != phys && gbhasedge(s, virt, regmap[colourmap[phys]][i])) {
			return 0;
		}
	}
	return 1;
}

static void
alputedge(Isel *s, regid u, regid v)
{
	if (s->ngadj[u] == s->gadjsz[u]) {
		s->gadjsz[u] = s->gadjsz[u]*2 + 1;
		s->gadj[u] = xrealloc(s->gadj[u], s->gadjsz[u]*sizeof(regid));
	}
	s->gadj[u][s->ngadj[u]] = v;
	s->ngadj[u]++;
}

static void
wlput(Loc ***wl, size_t *nwl, Loc *l)
{
	lappend(wl, nwl, l);
	l->list = wl;
}

static void
wldel(Isel *s, Loc ***wl, size_t *nwl, size_t idx)
{
	(*wl)[idx]->list = NULL;
	ldel(wl, nwl, idx);
}

static void
wlputset(Bitset *bs, regid r)
{
	bsput(bs, r);
	locmap[r]->list = bs;
}


static void
addedge(Isel *s, regid u, regid v)
{
	if (u == v || gbhasedge(s, u, v))
		return;
	if (u == Rrbp || u == Rrsp || u == Rrip)
		return;
	if (v == Rrbp || v == Rrsp || v == Rrip)
		return;
	if (rclass(locmap[u]) != rclass(locmap[v]))
		return;
	if (bshas(s->prepainted, u) && bshas(s->prepainted, v))
		return;

	gbputedge(s, u, v);
	gbputedge(s, v, u);
	if (!bshas(s->prepainted, u)) {
		alputedge(s, u, v);
		s->degree[u] += degreechange(s, v, u);
	}
	if (!bshas(s->prepainted, v)) {
		alputedge(s, v, u);
		s->degree[v] += degreechange(s, u, v);
	}
}

static void
gfree(Isel *s)
{
	size_t i;

	for (i = 0; i < s->nreg; i++)
		free(s->gadj[i]);
	free(s->gbits);
	free(s->gadj);
	free(s->ngadj);
}

static void
setup(Isel *s)
{
	size_t gchunks;
	size_t i;

	gfree(s);
	s->nreg = maxregid;
	gchunks = (s->nreg*s->nreg)/Sizetbits + 1;
	s->gbits = zalloc(gchunks*sizeof(size_t));
	/* fresh adj list repr. */
	s->gadj = zalloc(s->nreg * sizeof(regid*));
	s->ngadj = zalloc(s->nreg * sizeof(size_t));
	s->gadjsz = zalloc(s->nreg * sizeof(size_t));

	s->mactiveset = bsclear(s->mactiveset);
	s->wlmoveset = bsclear(s->wlmoveset);
	s->spilled = bsclear(s->spilled);
	s->coalesced = bsclear(s->coalesced);
	lfree(&s->wlspill, &s->nwlspill);
	lfree(&s->wlfreeze, &s->nwlfreeze);
	lfree(&s->wlsimp, &s->nwlsimp);

	free(s->aliasmap);
	free(s->degree);
	free(s->rmoves);
	free(s->nrmoves);

	s->aliasmap = zalloc(s->nreg * sizeof(Loc*));
	s->degree = zalloc(s->nreg * sizeof(int));
	s->nuses = zalloc(s->nreg * sizeof(int));
	s->rmoves = zalloc(s->nreg * sizeof(Insn**));
	s->nrmoves = zalloc(s->nreg * sizeof(size_t));

	for (i = 0; bsiter(s->prepainted, &i); i++)
		s->degree[i] = 1<<16;
}

static void
build(Isel *s)
{
	regid u[Nreg], d[Nreg];
	size_t nu, nd;
	size_t i, k, a;
	ssize_t j;
	regid *livesparse;
	regid *livedense;
	size_t nlive;
	regid  r;
	Asmbb **bb;
	size_t nbb;
	Insn *insn;
	size_t l;

	/* set up convenience vars */
	bb = s->bb;
	nbb = s->nbb;

#define PUT(reg) do { \
	assert(reg < maxregid); \
	if (livesparse[reg] >= nlive || livedense[livesparse[reg]] != reg) {\
		livesparse[reg] = nlive; \
		livedense[nlive] = reg; \
		nlive++; \
	} \
	} while(0)

#define DEL(reg) do { \
	assert(reg < maxregid); \
	regid rtmp; \
	if (livesparse[reg] < nlive && livedense[livesparse[reg]] == reg) { \
		rtmp = livedense[nlive - 1]; \
		livedense[livesparse[reg]] = rtmp; \
		livesparse[rtmp] = livesparse[reg]; \
		nlive--; \
	} \
	} while(0)

	/* sparse sets are used here because we iterate them. A lot. */
	livedense = zalloc((maxregid + 1) * sizeof(regid));
	livesparse = zalloc((maxregid + 1) * sizeof(regid));
	for (i = 0; i < nbb; i++) {
		if (!bb[i])
			continue;
		nlive = 0;
		for (k = 0; bsiter(bb[i]->liveout, &k); k++)
			PUT(k);
		for (j = bb[i]->ni - 1; j >= 0; j--) {
			insn = bb[i]->il[j];
			nu = uses(insn, u);
			nd = defs(insn, d);

			/* add these to the initial set */
			for (k = 0; k < nu; k++) {
				if (!bshas(s->prepainted, u[k])) {
					wlputset(s->initial, u[k]);
					s->nuses[u[k]]++;
				}
			}
			for (k = 0; k < nd; k++) {
				if (!bshas(s->prepainted, d[k]))
					wlputset(s->initial, d[k]);
			}

			/* moves get special treatment, since we don't want spuriou
			 * edges between the src and dest */
			if (ismove(insn)) {
				/* live \= uses(i) */
				for (k = 0; k < nu; k++) {
					/* remove all physical register aliases */
					if (bshas(s->prepainted, u[k])) {
						for (a = 0; a < Nmode; a++) {
							r = regmap[colourmap[u[k]]][a];
							DEL(r);
						}
					} else {
						DEL(u[k]);
					}
				}

				for (k = 0; k < nu; k++)
					lappend(&s->rmoves[u[k]], &s->nrmoves[u[k]], insn);
				for (k = 0; k < nd; k++)
					lappend(&s->rmoves[d[k]], &s->nrmoves[d[k]], insn);
				lappend(&s->wlmove, &s->nwlmove, insn);
				bsput(s->wlmoveset, insn->uid);
			}
			/* live = live U def(i) */
			for (k = 0; k < nd; k++) {
				PUT(d[k]);
			}

			for (k = 0; k < nd; k++) {
				for (l = 0; l < nlive; l++) {
					addedge(s, d[k], livedense[l]);
				}
			}
			/* live = use(i) U (live \ def(i)) */
			for (k = 0; k < nd; k++) {
				DEL(d[k]);
			}

			for (k = 0; k < nu; k++) {
				PUT(u[k]);
			}
		}
	}
	free(livedense);
	free(livesparse);
#undef PUT
#undef DEL
}

static int
adjavail(Isel *s, regid r)
{
	if (locmap[r]->list == &s->selstk)
		return 0;
	if (bshas(s->coalesced, r))
		return 0;
	return 1;
}

static size_t
nodemoves(Isel *s, regid n, Insn ***pil)
{
	size_t i, count;
	regid rid;
	Insn **il;

	count = 0;
	il = malloc(s->nrmoves[n] * sizeof(Insn*));
	for (i = 0; i < s->nrmoves[n]; i++) {
		rid = s->rmoves[n][i]->uid;
		if (bshas(s->mactiveset, rid) || bshas(s->wlmoveset, rid))
			il[count++] = s->rmoves[n][i];
	}
	*pil = il;
	return count;
}


static int
moverelated(Isel *s, regid n)
{
	size_t i;

	for (i = 0; i < s->nrmoves[n]; i++) {
		if (bshas(s->mactiveset, s->rmoves[n][i]->uid))
			return 1;
		if (bshas(s->wlmoveset, s->rmoves[n][i]->uid))
			return 1;
	}
	return 0;
}

static void
mkworklist(Isel *s)
{
	size_t i;

	for (i = 0; bsiter(s->initial, &i); i++) {
		if (bshas(s->prepainted, i))
			continue;
		else if (!istrivial(s, i))
			wlput(&s->wlspill, &s->nwlspill, locmap[i]);
		else if (moverelated(s, i)) {
			wlput(&s->wlfreeze, &s->nwlfreeze, locmap[i]);
		}
		else
			wlput(&s->wlsimp, &s->nwlsimp, locmap[i]);
		locmap[i]->reg.colour = 0;
	}
}

static void
enablemove(Isel *s, regid n)
{
	size_t i, j;
	Insn **il;
	size_t ni;

	ni = nodemoves(s, n, &il);
	for (i = 0; i < ni; i++) {
		if (!bshas(s->mactiveset, il[i]->uid))
			continue;
		for (j = 0; j < s->nmactive; j++) {
			if (il[i] == s->mactive[j]) {
				ldel(&s->mactive, &s->nmactive, j);
				lappend(&s->wlmove, &s->nwlmove, il[i]);
				bsdel(s->mactiveset, il[i]->uid);
				bsput(s->wlmoveset, il[i]->uid);
			}
		}
	}
	free(il);
}

static void
decdegree(Isel *s, regid m)
{
	int before, after;
	int found;
	size_t idx, i;
	regid n;

	assert(m < s->nreg);
	before = istrivial(s, m);
	s->degree[m]--;
	after = istrivial(s, m);

	if (before != after) {
		enablemove(s, m);
		for (i = 0; i < s->ngadj[m]; i++) {
			n = s->gadj[m][i];
			if (adjavail(s, n))
				enablemove(s, n);
		}

		/* Subtle:
		 *
		 * If this code is being called from coalesce(),
		 * then the degree could have been bumped up only
		 * temporarily. This means that the node can already
		 * be on wlfreeze or wlsimp.
		 *
		 * Therefore, if we don't find it on wlspill, we assert
		 * that the node is already on the list that we'd be
		 * moving it to.
		 */
		found = wlfind(s->wlspill, s->nwlspill, m, &idx);
		if (found)
			wldel(s, &s->wlspill, &s->nwlspill, idx);
		if (moverelated(s, m)) {
			if (!found) {
				assert(wlfind(s->wlfreeze, s->nwlfreeze, m, &idx) != 0);
			} else {
				wlput(&s->wlfreeze, &s->nwlfreeze, locmap[m]);
			}
		} else {
			if (!found) {
				assert(wlfind(s->wlsimp, s->nwlsimp, m, &idx));
			} else {
				wlput(&s->wlsimp, &s->nwlsimp, locmap[m]);
			}
		}
	}
}

static void
simp(Isel *s)
{
	Loc *l;
	regid m;
	size_t i;

	l = lpop(&s->wlsimp, &s->nwlsimp);
	wlput(&s->selstk, &s->nselstk, l);
	for (i = 0; i < s->ngadj[l->reg.id]; i++) {
		m = s->gadj[l->reg.id][i];
		if (adjavail(s, m))
			decdegree(s, m);
	}
}

static regid
getmappedalias(Loc **aliasmap, size_t nreg, regid id)
{
	/*
	 * if we get called from rewrite(), we can get a register that
	 * we just created, with an id bigger than the number of entrie
	 * in the alias map. We should just return its id in that case.
	 */
	while (id < nreg) {
		if (!aliasmap[id])
			break;
		id = aliasmap[id]->reg.id;
	};
	return id;
}

static regid
getalias(Isel *s, regid id)
{
	return getmappedalias(s->aliasmap, s->nreg, id);
}

static void
wladd(Isel *s, regid u)
{
	size_t i;

	if (bshas(s->prepainted, u))
		return;
	if (moverelated(s, u))
		return;
	if (!istrivial(s, u))
		return;

	assert(locmap[u]->list == &s->wlfreeze || locmap[u]->list == &s->wlsimp);
	if (wlfind(s->wlfreeze, s->nwlfreeze, u, &i))
		wldel(s, &s->wlfreeze, &s->nwlfreeze, i);
	wlput(&s->wlsimp, &s->nwlsimp, locmap[u]);
}

static int
conservative(Isel *s, regid u, regid v)
{
	int k;
	size_t i;
	regid n;

	k = 0;
	for (i = 0; i < s->ngadj[u]; i++) {
		n = s->gadj[u][i];
		if (adjavail(s, n) && !istrivial(s, n))
			k++;
	}
	for (i = 0; i < s->ngadj[v]; i++) {
		n = s->gadj[v][i];
		if (adjavail(s, n) && !istrivial(s, n))
			k++;
	}
	return k < _K[rclass(locmap[u])];
}

/* FIXME: is this actually correct? */
static int
ok(Isel *s, regid t, regid r)
{
	return istrivial(s, t) || bshas(s->prepainted, t) || gbhasedge(s, t, r);
}

static int
combinable(Isel *s, regid u, regid v)
{
	regid t;
	size_t i;

	/* Regs of different modes can't be combined as things stand.
	 * In principle they should be combinable, but it confused the
	 * whole mode dance. */
	if (locmap[u]->mode != locmap[v]->mode)
		return 0;
	/* if u isn't prepainted, can we conservatively coalesce? */
	if (!bshas(s->prepainted, u) && conservative(s, u, v))
		return 1;

	/* if it is, are the adjacent nodes ok to combine with this? */
	for (i = 0; i < s->ngadj[v]; i++) {
		t = s->gadj[v][i];
		if (adjavail(s, t) && !ok(s, t, u))
			return 0;
	}
	return 1;
}

static void
combine(Isel *s, regid u, regid v)
{
	regid t;
	size_t idx;
	size_t i, j;
	int has;

	if (debugopt['r'] > 2)
		printedge(stdout, "combining:", u, v);
	if (wlfind(s->wlfreeze, s->nwlfreeze, v, &idx))
		wldel(s, &s->wlfreeze, &s->nwlfreeze, idx);
	else if (wlfind(s->wlspill, s->nwlspill, v, &idx)) {
		wldel(s, &s->wlspill, &s->nwlspill, idx);
	}
	wlputset(s->coalesced, v);
	s->aliasmap[v] = locmap[u];
	s->nuses[u] += s->nuses[v];

	/* nodemoves[u] = nodemoves[u] U nodemoves[v] */
	for (i = 0; i < s->nrmoves[v]; i++) {
		has = 0;
		for (j = 0; j < s->nrmoves[u]; j++) {
			if (s->rmoves[v][i] == s->rmoves[u][j]) {
				has = 1;
				break;
			}
		}
		if (!has)
			lappend(&s->rmoves[u], &s->nrmoves[u], s->rmoves[v][i]);
	}

	for (i = 0; i < s->ngadj[v]; i++) {
		t = s->gadj[v][i];
		if (!adjavail(s, t))
			continue;
		if (debugopt['r'] > 2)
			printedge(stdout, "combine-putedge:", t, u);
		addedge(s, t, u);
		decdegree(s, t);
	}
	if (!istrivial(s, u) && wlfind(s->wlfreeze, s->nwlfreeze, u, &idx)) {
		wldel(s, &s->wlfreeze, &s->nwlfreeze, idx);
		wlput(&s->wlspill, &s->nwlspill, locmap[u]);
	}
}

static int
constrained(Isel *s, regid u, regid v)
{
	size_t i;

	if (bshas(s->prepainted, v))
		return 1;
	if (bshas(s->prepainted, u))
		for (i = 0; i < Nmode; i++)
			if (regmap[colourmap[u]][i] && gbhasedge(s, regmap[colourmap[u]][i], v))
				return 1;
	return gbhasedge(s, u, v);
}

static void
coalesce(Isel *s)
{
	Insn *m;
	regid u, v, tmp;

	m = lpop(&s->wlmove, &s->nwlmove);
	bsdel(s->wlmoveset, m->uid);
	u = getalias(s, m->args[0]->reg.id);
	v = getalias(s, m->args[1]->reg.id);

	if (bshas(s->prepainted, v)) {
		tmp = u;
		u = v;
		v = tmp;
	}

	if (u == v) {
		lappend(&s->mcoalesced, &s->nmcoalesced, m);
		wladd(s, u);
		wladd(s, v);
	} else if (constrained(s, u, v)) {
		lappend(&s->mconstrained, &s->nmconstrained, m);
		wladd(s, u);
		wladd(s, v);
	} else if (combinable(s, u, v)) {
		lappend(&s->mcoalesced, &s->nmcoalesced, m);
		combine(s, u, v);
		wladd(s, u);
	} else {
		lappend(&s->mactive, &s->nmactive, m);
		bsput(s->mactiveset, m->uid);
	}
}

static int
mldel(Insn ***ml, size_t *nml, Bitset *bs, Insn *m)
{
	size_t i;
	if (bshas(bs, m->uid)) {
		bsdel(bs, m->uid);
		for (i = 0; i < *nml; i++) {
			if (m == (*ml)[i]) {
				ldel(ml, nml, i);
				return 1;
			}
		}
	}
	return 0;
}

static void
freezemoves(Isel *s, Loc *u)
{
	size_t i;
	Insn **ml;
	Insn *m;
	size_t nml;
	size_t idx;
	Loc *v;

	nml = nodemoves(s, u->reg.id, &ml);
	for (i = 0; i < nml; i++) {
		m = ml[i];
		if (getalias(s, m->args[0]->reg.id) == getalias(s, u->reg.id))
			v = locmap[getalias(s, m->args[1]->reg.id)];
		else
			v = locmap[getalias(s, m->args[0]->reg.id)];

		if (!mldel(&s->mactive, &s->nmactive, s->mactiveset, m))
			mldel(&s->wlmove, &s->nwlmove, s->wlmoveset, m);

		lappend(&s->mfrozen, &s->nmfrozen, m);
		if (!moverelated(s, v->reg.id) && istrivial(s, v->reg.id)) {
			if (!wlfind(s->wlfreeze, s->nwlfreeze, v->reg.id, &idx))
				die("Reg %zd not in freeze wl\n", v->reg.id);
			wldel(s, &s->wlfreeze, &s->nwlfreeze, idx);
			wlput(&s->wlsimp, &s->nwlsimp, v);
		}

	}
	lfree(&ml, &nml);
}

static void
freeze(Isel *s)
{
	Loc *l;

	l = lpop(&s->wlfreeze, &s->nwlfreeze);
	wlput(&s->wlsimp, &s->nwlsimp, l);
	freezemoves(s, l);
}

/* Select the spill candidates */
static void
selspill(Isel *s)
{
	size_t i;
	Loc *m;

	/* FIXME: pick a better heuristic for spilling */
	m = NULL;
	for (i = 0; i < s->nwlspill; i++) {
		if (!bshas(s->shouldspill, s->wlspill[i]->reg.id))
			continue;
		m = s->wlspill[i];
		wldel(s, &s->wlspill, &s->nwlspill, i);
		break;
	}
	if (!m) {
		for (i = 0; i < s->nwlspill; i++) {
			if (bshas(s->neverspill, s->wlspill[i]->reg.id)) {
				continue;
			}
			m = s->wlspill[i];
			wldel(s, &s->wlspill, &s->nwlspill, i);
			break;
		}
	}
	assert(m != NULL);
	wlput(&s->wlsimp, &s->nwlsimp, m);
	freezemoves(s, m);
}

/*
 * Selects the colours for registers, spilling to the
 * stack if no free registers can be found.
 */
static int
paint(Isel *s)
{
	int taken[Nreg];
	Loc *n, *w;
	regid l;
	size_t i, j;
	int spilled;
	int found;

	spilled = 0;
	while (s->nselstk) {
		memset(taken, 0, Nreg*sizeof(int));
		n = lpop(&s->selstk, &s->nselstk);

		for (j = 0; j < s->ngadj[n->reg.id];j++) {
			l = s->gadj[n->reg.id][j];
			if (debugopt['r'] > 1)
				printedge(stdout, "paint-edge:", n->reg.id, l);
			w = locmap[getalias(s, l)];
			if (w->reg.colour)
				taken[colourmap[w->reg.colour]] = 1;
		}

		found = 0;
		for (i = 0; i < Northogonal; i++) {
			if (regmap[i][n->mode] && !taken[i]) {
				n->reg.colour = regmap[i][n->mode];
				found = 1;
				break;
			}
		}
		if (!found) {
			spilled = 1;
			wlputset(s->spilled, n->reg.id);
		}
	}
	for (l = 0; bsiter(s->coalesced, &l); l++) {
		n = locmap[getalias(s, l)];
		locmap[l]->reg.colour = n->reg.colour;
	}
	return spilled;
}

static Loc *
mapfind(Isel *s, Htab *map, Loc *old)
{
	Loc *new;
	Loc *base;
	Loc *idx;
	regid id;

	if (!old)
		return NULL;

	new = NULL;
	if (old->type == Locreg) {
		id = getalias(s, old->reg.id);
		new = htget(map, locmap[id]);
	} else if (old->type == Locmem || old->type == Locmeml) {
		base = old->mem.base;
		idx = old->mem.idx;
		if (base)
			base = locmap[getalias(s, base->reg.id)];
		if (idx)
			idx = locmap[getalias(s, idx->reg.id)];
		base = mapfind(s, map, base);
		idx = mapfind(s, map, idx);
		if (base != old->mem.base || idx != old->mem.idx) {
			if (old->type == Locmem)
				new = locmems(old->mem.constdisp, base, idx, old->mem.scale, old->mode);
			else
				new = locmemls(old->mem.lbldisp, base, idx, old->mem.scale, old->mode);
		}
	}
	if (new)
		return new;
	return old;
}

static Loc *
spillslot(Isel *s, regid reg)
{
	size_t stkoff;

	stkoff = ptoi(htget(s->spillslots, itop(reg)));
	return locmem(-stkoff, locphysreg(Rrbp), NULL, locmap[reg]->mode);
}

static void
updatelocs(Isel *s, Htab *map, Insn *insn)
{
	size_t i;

	for (i = 0; i < insn->nargs; i++) {
		insn->args[i] = mapfind(s, map, insn->args[i]);
		insn->args[i] = mapfind(s, map, insn->args[i]);
	}
}

/*
 * Takes two tables for remappings, of size Nreg/Nreg,
 * and fills them, storign the number of uses or defs. Return
 * whether there are any remappings at all.
 */
static int
remap(Isel *s, Htab *map, Insn *insn, regid *use, size_t nuse, regid *def, size_t ndef)
{
	regid ruse, rdef;
	int remapped;
	Loc *tmp;
	size_t i;

	remapped = 0;
	for (i = 0; i < nuse; i++) {
		ruse = getalias(s, use[i]);
		if (!bshas(s->spilled, ruse))
			continue;
		tmp = locreg(locmap[ruse]->mode);
		htput(map, locmap[ruse], tmp);
		bsput(s->neverspill, tmp->reg.id);
		remapped = 1;
	}

	for (i = 0; i < ndef; i++) {
		rdef = getalias(s, def[i]);
		if (!bshas(s->spilled, rdef))
			continue;
		if (hthas(map, locmap[rdef]))
			continue;
		tmp = locreg(locmap[rdef]->mode);
		htput(map, locmap[rdef], tmp);
		bsput(s->neverspill, tmp->reg.id);
		remapped = 1;
	}

	return remapped;
}

static int
nopmov(Isel *s, Insn *insn)
{
	if (insn->op != Imov)
		return 0;
	if (insn->args[0]->type != Locreg || insn->args[1]->type != Locreg)
		return 0;
	/* Prepainted register moves need to be kept live to avoid coalescing over them */
	return insn->args[0]->reg.id == insn->args[1]->reg.id && !bshas(s->prepainted, insn->args[0]->reg.id);
}

void
replacealias(Isel *s, Loc **map, size_t nreg, Insn *insn)
{
	size_t i;
	Loc *l;

	if (!map)
		return;
	for (i = 0; i < insn->nargs; i++) {
		l = insn->args[i];
		if (l->type == Locreg) {
			insn->args[i] = locmap[getalias(s, l->reg.id)];
		} else if (l->type == Locmem || l->type == Locmeml) {
			if (l->mem.base)
				l->mem.base = locmap[getalias(s, l->mem.base->reg.id)];
			if (l->mem.idx)
				l->mem.idx = locmap[getalias(s, l->mem.idx->reg.id)];
		}
	}
}

static ulong
reglochash(void *p)
{
	Loc *l;

	l = p;
	return inthash(l->reg.id);
}


static int
regloceq(void *pa, void *pb)
{
	Loc *a, *b;

	a = pa;
	b = pb;
	return a->reg.id == b->reg.id;
}
/*
 * Rewrite instructions using spilled registers, inserting
 * appropriate loads and stores into the BB
 */
static void
rewritebb(Isel *s, Asmbb *bb, Loc **aliasmap)
{
	regid use[Nreg], def[Nreg];
	size_t nuse, ndef;
	Insn *insn, *mov;
	size_t i, j;
	Insn **new;
	size_t nnew;
	Htab *map;
	Loc *tmp;

	new = NULL;
	nnew = 0;
	if (!bb)
		return;
	map = mkht(reglochash, regloceq);
	for (j = bb->ni; j > 0; j--) {
		insn = bb->il[j - 1];
		replacealias(s, aliasmap, s->nreg, insn);
		if (nopmov(s, insn))
			continue;
		nuse = uses(insn, use);
		ndef = defs(insn, def);
		/* if there is a remapping, insert the loads and stores as needed */
		if (remap(s, map, insn, use, nuse, def, ndef)) {
			for (i = 0; i < ndef; i++) {
				tmp = htget(map, locmap[def[i]]);
				if (!tmp)
					continue;
				if (isfloatmode(tmp->mode))
					mov = mkinsn(Imovs, tmp, spillslot(s, def[i]), NULL);
				else
					mov = mkinsn(Imov, tmp, spillslot(s, def[i]), NULL);
				lappend(&new, &nnew, mov);
			}
			updatelocs(s, map, insn);
			lappend(&new, &nnew, insn);
			for (i = 0; i < nuse; i++) {
				tmp = htget(map, locmap[use[i]]);
				if (!tmp)
					continue;
				if (isfloatmode(tmp->mode))
					mov = mkinsn(Imovs, spillslot(s, use[i]), tmp, NULL);
				else
					mov = mkinsn(Imov, spillslot(s, use[i]), tmp, NULL);
				lappend(&new, &nnew, mov);
			}
			for (i = 0; i < nuse; i++)
				htdel(map, locmap[use[i]]);
			for (i = 0; i < ndef; i++)
				htdel(map, locmap[def[i]]);
		} else {
			lappend(&new, &nnew, insn);
		}
	}
	for (i = 0; i < nnew/2; i++) {
		insn = new[i];
		new[i] = new[nnew - i - 1];
		new[nnew - i -1] = insn;
	}
	lfree(&bb->il, &bb->ni);
	bb->il = new;
	bb->ni = nnew;
}

static void
addspill(Isel *s, Loc *l)
{
	s->stksz->lit += modesize[l->mode];
	s->stksz->lit = align(s->stksz->lit, modesize[l->mode]);
	if (debugopt['r']) {
		printf("spill ");
		dbglocprint(stdout, l, 'x');
		printf(" to %zd(%%rbp)\n", s->stksz->lit);
	}
	htput(s->spillslots, itop(l->reg.id), itop(s->stksz->lit));
}

/*
 * Rewrites the function code so that it no longer contain
 * references to spilled registers. Every use of spilled reg
 *
 *      insn %rX,%rY
 *
 * is rewritten to look like:
 *
 *      mov 123(%rsp),%rZ
 *      insn %rZ,%rW
 *      mov %rW,234(%rsp)
 */
static void
rewrite(Isel *s, Loc **aliasmap)
{
	size_t i;

	s->spillslots = mkht(ptrhash, ptreq);
	/* set up stack locations for all spilled registers. */
	for (i = 0; bsiter(s->spilled, &i); i++)
		addspill(s, locmap[i]);

	/* rewrite instructions using them */
	for (i = 0; i < s->nbb; i++)
		rewritebb(s, s->bb[i], aliasmap);
	htfree(s->spillslots);
	bsclear(s->spilled);
}

/*
 * Coalescing registers leaves a lot
 * of moves that look like
 *
 *      mov %r123,%r123.
 *
 * This is useless. This deletes them.
 */
static void
delnops(Isel *s)
{
	Insn *insn;
	Asmbb *bb;
	Insn **new;
	size_t nnew;
	size_t i, j;

	for (i = 0; i < s->nbb; i++) {
		if (!s->bb[i])
			continue;
		new = NULL;
		nnew = 0;
		bb = s->bb[i];
		for (j = 0; j < bb->ni; j++) {
			insn = bb->il[j];
			if (ismove(insn) && insn->args[0]->reg.colour == insn->args[1]->reg.colour)
				continue;
			lappend(&new, &nnew, insn);
		}
		lfree(&bb->il, &bb->ni);
		bb->il = new;
		bb->ni = nnew;
	}
	if (debugopt['r'])
		dumpasm(s, stdout);
}

void
regalloc(Isel *s)
{
	int spilled;
	size_t i;
	Loc **aliasmap;

	/* Initialize the list of prepainted registers */
	s->prepainted = mkbs();
	bsput(s->prepainted, 0);
	for (i = 0; i < Nreg; i++)
		bsput(s->prepainted, i);

	s->shouldspill = mkbs();
	s->neverspill = mkbs();
	s->initial = mkbs();
	for (i = 0; i < s->nsaved; i++)
		bsput(s->shouldspill, s->calleesave[i]->reg.id);
	do {
		aliasmap = NULL;
		setup(s);
		liveness(s);
		build(s);
		mkworklist(s);
		if (debugopt['r'])
			dumpasm(s, stdout);
		do {
			if (s->nwlsimp)
				simp(s);
			else if (s->nwlmove)
				coalesce(s);
			else if (s->nwlfreeze)
				freeze(s);
			else if (s->nwlspill) {
				if (!aliasmap)
					aliasmap = memdup(s->aliasmap, s->nreg * sizeof(Loc*));
				selspill(s);
			}
		} while (s->nwlsimp || s->nwlmove || s->nwlfreeze || s->nwlspill);
		spilled = paint(s);
		if (spilled)
			rewrite(s, aliasmap);
	} while (spilled);
	delnops(s);
	bsfree(s->prepainted);
	bsfree(s->shouldspill);
	bsfree(s->neverspill);
	gfree(s);
}

void
wlprint(FILE *fd, char *name, Loc **wl, size_t nwl)
{
	size_t i;
	char *sep;

	sep = "";
	fprintf(fd, "%s = [", name);
	for (i = 0; i < nwl; i++) {
		fprintf(fd, "%s", sep);
		dbglocprint(fd, wl[i], 'x');
		fprintf(fd, "(%zd)", wl[i]->reg.id);
		sep = ",";
	}
	fprintf(fd, "]\n");
}

static void
setprint(FILE *fd, Bitset *s)
{
	char *sep;
	size_t i;

	sep = "";
	for (i = 0; i < bsmax(s); i++) {
		if (bshas(s, i)) {
			fprintf(fd, "%s%zd", sep, i);
			sep = ",";
		}
	}
	fprintf(fd, "\n");
}

static void
locsetprint(FILE *fd, Bitset *s)
{
	char *sep;
	size_t i;

	sep = "";
	for (i = 0; i < bsmax(s); i++) {
		if (bshas(s, i)) {
			fprintf(fd, "%s", sep);
			dbglocprint(fd, locmap[i], 'x');
			sep = ",";
		}
	}
	fprintf(fd, "\n");
}

static void
printedge(FILE *fd, char *msg, size_t a, size_t b)
{
	fprintf(fd, "\t%s ", msg);
	dbglocprint(fd, locmap[a], 'x');
	fprintf(fd, " -- ");
	dbglocprint(fd, locmap[b], 'x');
	fprintf(fd, "\n");
}

void
dumpasm(Isel *s, FILE *fd)
{
	size_t i, j;
	char *sep;
	Asmbb *bb;

	fprintf(fd, "function %s\n", s->name);
	fprintf(fd, "WORKLISTS -- \n");
	wlprint(stdout, "spill", s->wlspill, s->nwlspill);
	wlprint(stdout, "simp", s->wlsimp, s->nwlsimp);
	wlprint(stdout, "freeze", s->wlfreeze, s->nwlfreeze);
	/* noisy to dump this all the time; only dump for higher debug levels */
	if (debugopt['r'] > 2) {
		fprintf(fd, "IGRAPH ----- \n");
		for (i = 0; i < s->nreg; i++) {
			for (j = i; j < s->nreg; j++) {
				if (gbhasedge(s, i, j))
					printedge(stdout, "", i, j);
			}
		}
	}
	fprintf(fd, "ASM -------- \n");
	for (j = 0; j < s->nbb; j++) {
		bb = s->bb[j];
		if (!bb)
			continue;
		fprintf(fd, "\n");
		fprintf(fd, "Bb: %d labels=(", bb->id);
		sep = "";
		for (i = 0; i < bb->nlbls; i++) {;
			fprintf(fd, "%s%s", bb->lbls[i], sep);
			sep = ",";
		}
		fprintf(fd, ")\n");

		fprintf(fd, "Pred: ");
		setprint(fd, bb->pred);
		fprintf(fd, "Succ: ");
		setprint(fd, bb->succ);

		fprintf(fd, "Use: ");
		locsetprint(fd, bb->use);
		fprintf(fd, "Def: ");
		locsetprint(fd, bb->def);
		fprintf(fd, "Livein:  ");
		locsetprint(fd, bb->livein);
		fprintf(fd, "Liveout: ");
		locsetprint(fd, bb->liveout);
		for (i = 0; i < bb->ni; i++)
			dbgiprintf(fd, bb->il[i]);
	}
	fprintf(fd, "ENDASM -------- \n");
}

