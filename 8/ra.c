#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "parse.h"
#include "opt.h"
#include "asm.h"

#define Sizetbits (CHAR_BIT*sizeof(size_t)) /* used in graph reprs */

typedef struct Usage Usage;
struct Usage {
    int l[Maxarg + 1];
    int r[Maxarg + 1];
};

Usage usetab[] = {
#define Use(...) {__VA_ARGS__}
#define Insn(i, fmt, use, def) use,
#include "insns.def"
#undef Insn
#undef Use
};

Usage deftab[] = {
#define Def(...) {__VA_ARGS__}
#define Insn(i, fmt, use, def) def,
#include "insns.def"
#undef Insn
#undef Def
};

static size_t uses(Insn *insn, long *u)
{
    size_t i, j;
    int k;
    Loc *m;

    j = 0;
    /* Add all the registers used and defined. Duplicates
     * in this list are fine, since they're being added to
     * a set anyways */
    for (i = 0; i < Maxarg; i++) {
	if (!usetab[insn->op].l[i])
	    break;
	k = usetab[insn->op].l[i] - 1;
	/* non-registers are handled later */
	if (insn->args[k]->type == Locreg)
	    u[j++] = insn->args[k]->reg.id;
    }
    /* some insns don't reflect their defs in the args.
     * These are explictly listed in the insn description */
    for (i = 0; i < Maxarg; i++) {
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
	u[j++] = m->mem.base->reg.id;
	if (m->mem.idx)
	    u[j++] = m->mem.idx->reg.id;
    }
    return j;
}

static size_t defs(Insn *insn, long *d)
{
    size_t i, j;
    int k;

    j = 0;
    /* Add all the registers dsed and defined. Duplicates
     * in this list are fine, since they're being added to
     * a set anyways */
    for (i = 0; i < Maxarg; i++) {
	if (!deftab[insn->op].l[i])
	    break;
	k = deftab[insn->op].l[i] - 1;
	if (insn->args[k]->type == Locreg)
	    d[j++] = insn->args[k]->reg.id;
    }
    /* some insns don't reflect their defs in the args.
     * These are explictly listed in the insn description */
    for (i = 0; i < Maxarg; i++) {
	if (!deftab[insn->op].r[i])
	    break;
	/* not a leak; physical registers get memoized */
	d[j++] = locphysreg(deftab[insn->op].r[i])->reg.id;
    }
    return j;
}

static void udcalc(Asmbb *bb)
{
    /* up to 2 registers per memloc, so 
     * 2*Maxarg is the maximum number of
     * uses or defs we can see */
    long   u[2*Maxarg], d[2*Maxarg];
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

static void liveness(Isel *s)
{
    Bitset *old;
    Asmbb **bb;
    size_t nbb;
    size_t i, j;
    int changed;

    bb = s->bb;
    nbb = s->nbb;
    for (i = 0; i < nbb; i++) {
	udcalc(s->bb[i]);
	bb[i]->livein = bsclear(bb[i]->livein);
	bb[i]->liveout = bsclear(bb[i]->liveout);
    }

    changed = 1;
    while (changed) {
	changed = 0;
	for (i = 0; i < nbb; i++) {
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
	}
    }
}

static int ismove(Insn *i)
{
    return i->op == Imov;
}

/* static */ int gbhasedge(size_t *g, size_t u, size_t v)
{
    size_t i;
    i = (maxregid * v) + u;
    return g[i/Sizetbits] & (1ULL <<(i % Sizetbits));
}

static void gbputedge(size_t *g, size_t u, size_t v)
{
    size_t i, j;
    i = (maxregid * v) + u;
    j = (maxregid * u) + v;
    g[i/Sizetbits] |= 1ULL <<(i % Sizetbits);
    g[j/Sizetbits] |= 1ULL <<(j % Sizetbits);
    assert(!gbhasedge(g, 5, 5));
}

static void addedge(Isel *s, size_t u, size_t v)
{
    if (u == v)
	return;
    gbputedge(s->gbits, u, v);
    if (!bshas(s->prepainted, u)) {
	bsput(s->gadj[u], v);
	s->degree[u]++;
    }
    if (!bshas(s->prepainted, v)) {
	bsput(s->gadj[v], u);
	s->degree[v]++;
    }
}

void setup(Isel *s)
{
    Bitset **gadj;
    size_t gchunks;
    size_t i;

    free(s->gbits);
    gchunks = (maxregid*maxregid)/Sizetbits + 1;
    s->gbits = zalloc(gchunks*sizeof(size_t));
    /* fresh adj list repr. try to avoid reallocating all the bitsets */
    gadj = zalloc(maxregid * sizeof(Bitset*));
    if (s->gadj)
	for (i = 0; i < maxregid; i++)
	    gadj[i] = bsclear(s->gadj[i]);
    else
	for (i = 0; i < maxregid; i++)
	    gadj[i] = mkbs();
    free(s->gadj);
    s->gadj = gadj;

    s->prepainted = bsclear(s->prepainted);
    s->degree = zalloc(maxregid * sizeof(int));
    s->moves = zalloc(maxregid * sizeof(Loc **));
    s->nmoves = zalloc(maxregid * sizeof(size_t));
}

static void build(Isel *s)
{
    long u[2*Maxarg], d[2*Maxarg];
    size_t nu, nd;
    size_t i, k;
    ssize_t j;
    Bitset *live;
    Asmbb **bb;
    size_t nbb;
    Insn *insn;
    size_t l;

    setup(s);
    /* set up convenience vars */
    bb = s->bb;
    nbb = s->nbb;

    //s->graph = zalloc(maxregid * sizeof(size_t));
    for (i = 0; i < nbb; i++) {
	live = bsdup(bb[i]->liveout);
	for (j = bb[i]->ni - 1; j >= 0; j--) {
	    insn = bb[i]->il[j];
	    nu = uses(insn, u);
	    nd = defs(insn, d);
	    if (ismove(insn)) {
		/* live \= uses(i) */
		for (k = 0; k < nu; k++)
		    bsdel(live, u[k]);

		for (k = 0; k < nu; k++)
		    lappend(&s->moves[u[k]], &s->nmoves[u[k]], insn);
		for (k = 0; k < nd; k++)
		    lappend(&s->moves[d[k]], &s->nmoves[d[k]], insn);
		lappend(&s->wlmove, &s->nwlmove, insn);
	    }
	    for (k = 0; k < nd; k++)
		bsput(live, d[k]);

	    for (k = 0; k < nd; k++)
		for (l = 0; bsiter(live, &l); l++)
		    addedge(s, d[k], l);
	}
    }
}

Bitset *adjacent(Isel *s, size_t n)
{
    Bitset *r;

    r = bsdup(s->gadj[n]);
    bsdiff(r, s->selstk);
    bsdiff(r, s->coalesced);
    return r;
}

size_t nodemoves(Isel *s, size_t nid, Insn ***pil)
{
    size_t i, j;
    size_t n;

    /* FIXME: inefficient. Do I care? */
    n = 0;
    for (i = 0; i < s->nmoves[nid]; i++) {
	for (j = 0; j < s->nactivemoves; j++) {
	    if (s->activemoves[j] == s->moves[nid][i]) {
		if (pil)
		    lappend(pil, &n, s->moves[nid][i]);
		continue;
	    }
	}
	for (j = 0; j < s->nwlmove; j++) {
	    if (s->wlmove[j] == s->moves[nid][i]) {
		if (pil)
		    lappend(pil, &n, s->moves[nid][i]);
		continue;
	    }
	}
    }
    return n;
}

static int moverelated(Isel *s, size_t n)
{
    return nodemoves(s, n, NULL) != 0;
}

/* static */ void mkworklist(Isel *s)
{
    size_t i;

    for (i = 0; i < maxregid; i++) {
	if (s->degree[i] >= K)
	    lappend(&s->wlspill, &s->nwlspill, locmap[i]);
	else if (moverelated(s, i))
	    lappend(&s->wlfreeze, &s->nwlfreeze, locmap[i]);
	else
	    lappend(&s->wlsimp, &s->nwlsimp, locmap[i]);
    }
}

void simp(Isel *s)
{
}

void coalesce(Isel *s)
{
}

void freeze(Isel *s)
{
}

void spill(Isel *s)
{
}

void regalloc(Isel *s)
{
    liveness(s);
    build(s);
    if (debug)
	dumpasm(s, stdout);
    do {
	if (s->nwlsimp)
	    simp(s);
	else if (s->nwlmove)
	    coalesce(s);
	else if (s->nwlfreeze)
	    freeze(s);
	else if (s->nwlspill)
	    spill(s);
	break;
    } while (s->nwlsimp || s->nwlmove || s->nwlfreeze || s->nwlspill);
    mkworklist(s);
}

static void setprint(FILE *fd, Bitset *s)
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

static void locsetprint(FILE *fd, Bitset *s)
{
    char *sep;
    size_t i;

    sep = "";
    for (i = 0; i < bsmax(s); i++) {
	if (bshas(s, i)) {
	    fprintf(fd, "%s", sep);
	    locprint(fd, locmap[i]);
	    sep = ",";
	}
    }
    fprintf(fd, "\n");
}

void dumpasm(Isel *s, FILE *fd)
{
    size_t i, j;
    char *sep;
    Asmbb *bb;

    fprintf(fd, "IGRAPH ----- \n");
    for (i = 0; i < maxregid; i++) {
	for (j = i; j < maxregid; j++) {
	    if (gbhasedge(s->gbits, i, j)) {
		locprint(fd, locmap[i]);
		fprintf(fd, " -- ");
		locprint(fd, locmap[j]);
		fprintf(fd, "\n");
	    }
	}
    }
    fprintf(fd, "ASM -------- \n");
    for (j = 0; j < s->nbb; j++) {
        bb = s->bb[j];
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
            iprintf(fd, bb->il[i]);
    }
    fprintf(fd, "ENDASM -------- \n");
}

