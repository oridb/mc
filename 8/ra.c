#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "opt.h"
#include "asm.h"

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

size_t uses(Insn *insn, long *u)
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

size_t defs(Insn *insn, long *d)
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

void usedef(Asmbb *bb)
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

void liveness(Isel *s)
{
    Bitset *old;
    Asmbb **bb;
    size_t nbb;
    size_t i, j;
    int changed;

    bb = s->bb;
    nbb = s->nbb;
    for (i = 0; i < nbb; i++) {
	usedef(s->bb[i]);
	bb[i]->livein = bsclear(bb[i]->livein);
	bb[i]->liveout = bsclear(bb[i]->liveout);
    }

    changed = 1;
    while (changed) {
	changed = 0;
	for (i = 0; i < nbb; i++) {
	    old = bsdup(bb[i]->liveout);
	    /* liveout[b] = U(s in succ) livein[s] */
	    for (j = 0; j < bsmax(bb[i]->succ); j++) {
		if (!bshas(bb[i]->succ, j))
		    continue;
		bsunion(bb[i]->liveout, bb[j]->livein);
	    }
	    /* livein[b] = use[b] U (out[b] \ def[b]) */
	    bb[i]->livein = bsclear(bb[i]->livein);
	    bsunion(bb[i]->livein, bb[i]->liveout);
	    bsdiff(bb[i]->liveout, bb[i]->def);
	    bsunion(bb[i]->liveout, bb[i]->use);
	    if (!changed)
		changed = !bseq(old, bb[i]->liveout);
	}
    }
}

int ismove(Insn *i)
{
    return i->op == Imov;
}

void addedge(Isel *s, int u, int v)
{
}

void build(Isel *s)
{
    /* uses/defs */
    long u[2*Maxarg], d[2*Maxarg];
    size_t nu, nd;
    /* indexes */
    size_t i, k;
    ssize_t j;
    /* liveness */
    Bitset *live;
    /* convenience vars */
    Asmbb **bb;
    size_t nbb;
    Insn *insn;
    uint l;

    bb = s->bb;
    nbb = s->nbb;
    s->moves = zalloc(maxregid * sizeof(Loc **));
    s->nmoves = zalloc(maxregid * sizeof(size_t));
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

void regalloc(Isel *s)
{
    liveness(s);
    if (debug)
	dumpasm(s->bb, s->nbb, stdout);
    build(s);
}

void setprint(FILE *fd, Bitset *s)
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

void locsetprint(FILE *fd, Bitset *s)
{
    char *sep;
    size_t i;

    sep = "";
    for (i = 0; i < bsmax(s); i++) {
	if (bshas(s, i)) {
	    fprintf(fd, "%s", sep);
	    locprint(fd, loctab[i]);
	    sep = ",";
	}
    }
    fprintf(fd, "\n");
}

void dumpasm(Asmbb **bbs, size_t nbb, FILE *fd)
{
    size_t i, j;
    char *sep;
    Asmbb *bb;

    fprintf(fd, "ASM -------- \n");
    for (j = 0; j < nbb; j++) {
        bb = bbs[j];
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

