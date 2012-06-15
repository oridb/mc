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

Reg regmap[6][Nmode] = {
    [0] = {Rnone, Ral, Rax, Reax},
    [1] = {Rnone, Rcl, Rcx, Recx},
    [2] = {Rnone, Rdl, Rdx, Redx},
    [3] = {Rnone, Rbl, Rbx, Rebx},
    [4] = {Rnone, Rnone, Rnone, Resp},
    [5] = {Rnone, Rnone, Rnone, Rebp},
};

int colourmap[Nreg] = {
    /* byte */
    [Ral] = 0,
    [Rcl] = 1,
    [Rdl] = 2,
    [Rbl] = 3,

    /* word */
    [Rax] = 0,
    [Rcx] = 1,
    [Rdx] = 2,
    [Rbx] = 3,

    /* dword */
    [Reax] = 0,
    [Recx] = 1,
    [Redx] = 2,
    [Rebx] = 3,
    [Resp] = 4,
    [Rebp] = 5,
};

/* %esp, %ebp are not in the allocatable pool */
static int isfixreg(Loc *l)
{
    if (l->reg.colour == Resp)
	return 1;
    if (l->reg.colour == Rebp)
	return 1;
    return 0;
}

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
	    if (!isfixreg(insn->args[k]))
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
	if (!isfixreg(m->mem.base))
	    u[j++] = m->mem.base->reg.id;
	if (m->mem.idx)
	    if (!isfixreg(m->mem.base))
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
	    if (!isfixreg(insn->args[k]))
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

/* we're only interested in register->register moves */
static int ismove(Insn *i)
{
    if (i->op != Imov)
	return 0;
    return i->args[0]->type == Locreg && i->args[1]->type == Locreg;
}

/* static */ int gbhasedge(Isel *s, size_t u, size_t v)
{
    size_t i;
    i = (maxregid * v) + u;
    return (s->gbits[i/Sizetbits] & (1ULL <<(i % Sizetbits))) != 0;
}

static void gbputedge(Isel *s, size_t u, size_t v)
{
    size_t i, j;

    i = (maxregid * u) + v;
    j = (maxregid * v) + u;
    s->gbits[i/Sizetbits] |= 1ULL <<(i % Sizetbits);
    s->gbits[j/Sizetbits] |= 1ULL <<(j % Sizetbits);
    assert(gbhasedge(s, u, v) && gbhasedge(s, v, u));
}

static void addedge(Isel *s, size_t u, size_t v)
{
    if (u == v || gbhasedge(s, u, v))
	return;
    gbputedge(s, u, v);
    gbputedge(s, v, u);
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

    s->spilled = bsclear(s->spilled);
    s->coalesced = bsclear(s->coalesced);
    /*
    s->wlspill = bsclear(s->wlspill);
    s->wlfreeze = bsclear(s->wlfreeze);
    s->wlsimp = bsclear(s->wlsimp);
    */

    s->aliasmap = zalloc(maxregid * sizeof(size_t));
    s->degree = zalloc(maxregid * sizeof(int));
    s->rmoves = zalloc(maxregid * sizeof(Loc **));
    s->nrmoves = zalloc(maxregid * sizeof(size_t));
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

    /* set up convenience vars */
    bb = s->bb;
    nbb = s->nbb;

    for (i = 0; i < nbb; i++) {
	live = bsdup(bb[i]->liveout);
	for (j = bb[i]->ni - 1; j >= 0; j--) {
	    insn = bb[i]->il[j];
	    nu = uses(insn, u);
	    nd = defs(insn, d);

	    /* moves get special treatment, since we don't want spurious
	     * edges between the src and dest */
	    if (ismove(insn)) {
		/* live \= uses(i) */
		for (k = 0; k < nu; k++)
		    bsdel(live, u[k]);

		for (k = 0; k < nu; k++)
		    lappend(&s->rmoves[u[k]], &s->nrmoves[u[k]], insn);
		for (k = 0; k < nd; k++)
		    lappend(&s->rmoves[d[k]], &s->nrmoves[d[k]], insn);
		lappend(&s->wlmove, &s->nwlmove, insn);
	    }
	    /* live = live U def(i) */
	    for (k = 0; k < nd; k++)
		bsput(live, d[k]);

	    for (k = 0; k < nd; k++)
		for (l = 0; bsiter(live, &l); l++)
		    addedge(s, d[k], l);
	    /* live = use(i) U (live \ def(i)) */
	    for (k = 0; k < nd; k++)
		bsdel(live, d[k]);
	    for (k = 0; k < nu; k++)
		bsput(live, u[k]);
	}
    }
}

Bitset *adjacent(Isel *s, regid n)
{
    Bitset *r;
    size_t i;

    r = bsdup(s->gadj[n]);
    for (i = 0; i < s->nselstk; i++)
	bsdel(r, s->selstk[i]->reg.id);
    bsdiff(r, s->coalesced);
    return r;
}

size_t nodemoves(Isel *s, regid n, Insn ***pil)
{
    size_t i, j;
    size_t count;

    /* FIXME: inefficient. Do I care? */
    count = 0;
    if (pil)
	*pil = NULL;
    for (i = 0; i < s->nrmoves[n]; i++) {
	for (j = 0; j < s->nmactive; j++) {
	    if (s->mactive[j] == s->rmoves[n][i]) {
		if (pil)
		    lappend(pil, &count, s->rmoves[n][i]);
		continue;
	    }
	}
	for (j = 0; j < s->nwlmove; j++) {
	    if (s->wlmove[j] == s->rmoves[n][i]) {
		if (pil)
		    lappend(pil, &count, s->rmoves[n][i]);
		continue;
	    }
	}
    }
    return count;
}

static int moverelated(Isel *s, regid n)
{
    return nodemoves(s, n, NULL) != 0;
}

/* static */ void mkworklist(Isel *s)
{
    size_t i;

    for (i = 0; i < maxregid; i++) {
	if (bshas(s->prepainted, i))
	    continue;
	else if (s->degree[i] >= K)
	    lappend(&s->wlspill, &s->nwlspill, locmap[i]);
	else if (moverelated(s, i))
	    lappend(&s->wlfreeze, &s->nwlfreeze, locmap[i]);
	else
	    lappend(&s->wlsimp, &s->nwlsimp, locmap[i]);
    }
}

void enablemove(Isel *s, regid n)
{
    size_t i, j;
    Insn **il;
    size_t ni;

    ni = nodemoves(s, n, &il);
    for (i = 0; i < ni; i++) {
	for (j = 0; j < s->nmactive; j++) {
	    if (il[i] == s->mactive[j]) {
		ldel(&s->mactive, &s->nmactive, j);
		lappend(&s->wlmove, &s->nwlmove, il[i]);
	    }
	}
    }
}

void decdegree(Isel *s, regid n)
{
    int d;
    regid m;
    Bitset *adj;

    d = s->degree[n];
    s->degree[n]--;

    if (d == K) {
	enablemove(s, n);
	adj = adjacent(s, n);
	for (m = 0; bsiter(adj, &m); m++)
	    enablemove(s, n);
	bsfree(adj);
    }
}

void simp(Isel *s)
{
    Loc *l;
    Bitset *adj;
    regid m;

    l = lpop(&s->wlsimp, &s->nwlsimp);
    lappend(&s->selstk, &s->nselstk, l);
    adj = adjacent(s, l->reg.id);
    for (m = 0; bsiter(adj, &m); m++)
	decdegree(s, m);
    bsfree(adj);
}

regid getalias(Isel *s, regid id)
{
    while (1) {
	if (!s->aliasmap[id])
	    break;
	id = s->aliasmap[id]->reg.id;
    };
    return id;
}

void wladd(Isel *s, regid u)
{
    size_t i;

    if (bshas(s->prepainted, u))
	return;
    if (moverelated(s, u))
	return;
    if (s->degree[u] >= K)
	return;
    for (i = 0; i < s->nwlfreeze; i++)
	if (s->wlfreeze[i]->reg.id == u)
	    ldel(&s->wlfreeze, &s->nwlfreeze, i);
    lappend(&s->wlsimp, &s->nwlsimp, locmap[u]);
}

int conservative(Isel *s, regid u, regid v)
{
    int k;
    regid n;
    size_t i;
    Bitset *uadj;
    Bitset *vadj;

    uadj = adjacent(s, u);
    vadj = adjacent(s, u);
    k = 0;
    for (i = 0; bsiter(uadj, &n); i++)
	if (s->degree[n] >= K)
	    k++;
    for (i = 0; bsiter(vadj, &n); i++)
	if (s->degree[n] >= K)
	    k++;
    bsfree(uadj);
    bsfree(vadj);
    return k < K;
}

/* FIXME: is this actually correct? */
int ok(Isel *s, regid t, regid r)
{
    if (s->degree[t] >= K)
	return 0;
    if (!bshas(s->prepainted, t))
	return 0;
    if (!gbhasedge(s, t, r))
	return 0;
    return 1;
}

int combinable(Isel *s, regid u, regid v)
{
    regid t;
    Bitset *adj;

    /* if u isn't prepainted, can we conservatively coalesce? */
    if (!bshas(s->prepainted, u) && conservative(s, u, v))
	return 1;

    /* if it is, are the adjacent nodes ok to combine with this? */
    adj = adjacent(s, u);
    for (t = 0; bsiter(adj, &t); t++)
	if (!ok(s, t, u))
	    return 0;
    bsfree(adj);
    return 1;
}

int wlhas(Loc **wl, size_t nwl, regid v, size_t *idx)
{
    size_t i;

    for (i = 0; i < nwl; i++) {
	if (wl[i]->reg.id == v) { 
	    *idx = i;
	    return 1;
	}
    }
    return 0;
}

void combine(Isel *s, regid u, regid v)
{
    Bitset *adj;
    regid t;
    size_t idx;
    size_t i, j;
    int has;

    printf("Want to combine ");
    locprint(stdout, locmap[u]);
    printf(" with ");
    locprint(stdout, locmap[v]);
    printf("\n");
    if (wlhas(s->wlfreeze, s->nwlfreeze, v, &idx))
	ldel(&s->wlfreeze, &s->nwlfreeze, idx);
    else if (wlhas(s->wlspill, s->nwlspill, v, &idx))
	ldel(&s->wlspill, &s->nwlspill, idx);
    bsput(s->coalesced, v);
    s->aliasmap[v] = locmap[u];

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
    
    adj = adjacent(s, v);
    for (t = 0; bsiter(adj, &t); t++) {
	gbputedge(s, t, u);
	decdegree(s, t);
    }
    if (s->degree[u] >= K && wlhas(s->wlfreeze, s->nwlfreeze, u, &idx)) {
	ldel(&s->wlfreeze, &s->nwlfreeze, idx);
	lappend(&s->wlspill, &s->nwlspill, locmap[u]);
    }

}

void coalesce(Isel *s)
{
    Insn *m;
    regid u, v, tmp;

    m = lpop(&s->wlmove, &s->nwlmove);
    /* FIXME: src, dst? dst, src? Does it matter? */
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
    } else if (bshas(s->prepainted, v) || gbhasedge(s, u, v)) {
	lappend(&s->mconstrained, &s->nmconstrained, m);
	wladd(s, u);
	wladd(s, v);
    } else if (combinable(s, u, v)) {
	lappend(&s->mcoalesced, &s->nmcoalesced, m);
	combine(s, u, v);
	wladd(s, u);
    } else {
	lappend(&s->mactive, &s->nmactive, m);
    }
}

int mldel(Insn ***ml, size_t *nml, Insn *m)
{
    size_t i;
    for (i = 0; i < *nml; i++) {
	if (m == (*ml)[i]) {
	    ldel(ml, nml, i);
	    return 1;
	}
    }
    return 0;
}

void freezemoves(Isel *s, Loc *u)
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
	if (m->args[0] == u)
	    v = m->args[1];
	else if (m->args[1] == u)
	    v = m->args[0];
	else
	    continue;

	if (!mldel(&s->mactive, &s->nmactive, m))
	    mldel(&s->wlmove, &s->nwlmove, m);
	lappend(&s->mfrozen, &s->nmfrozen, m);
	if (!nodemoves(s, v->reg.id, NULL) && s->degree[v->reg.id] < K) {
	    if (!wlhas(s->wlfreeze, s->nwlfreeze, v->reg.id, &idx))
		die("Reg %zd not in freeze wl\n", v->reg.id);
	    ldel(&s->wlfreeze, &s->nwlfreeze, idx);
	    lappend(&s->wlsimp, &s->nwlsimp, v);
	}

    }
    lfree(&ml, &nml);
}

void freeze(Isel *s)
{
    Loc *l;

    l = lpop(&s->wlfreeze, &s->nwlfreeze);
    lappend(&s->wlsimp, &s->nwlsimp, l);
    freezemoves(s, l);
}

void selspill(Isel *s)
{
    size_t i;
    Loc *m;

    /* FIXME: pick a better heuristic for spilling */
    for (i = 0; i < s->nwlspill; i++) {
	if (!bshas(s->spilled, s->wlspill[i]->reg.id)) {
	    m = s->wlspill[i];
	    ldel(&s->wlspill, &s->nwlspill, i);
	    break;
	}
    }
    assert(m != NULL);
    lappend(&s->wlsimp, &s->nwlsimp, m);
    freezemoves(s, m);
}

int paint(Isel *s)
{
    int taken[K + 2]; /* esp, ebp aren't "real colours" */
    Bitset *adj;
    Loc *n, *w;
    regid l;
    size_t i;
    int spilled;
    int found;

    spilled = 0;
    while (s->nselstk) {
	bzero(taken, K*sizeof(int));
	n = lpop(&s->selstk, &s->nselstk);

	adj = adjacent(s, n->reg.id);
	for (l = 0; bsiter(adj, &l); l++) {
	    w = locmap[getalias(s, l)];
	    if (w->reg.colour)
		taken[colourmap[w->reg.colour]] = 1;
	}
	bsfree(adj);

	found = 0;
	for (i = 0; i < K; i++) {
	    if (!taken[i]) {
		locprint(stdout, n);
		printf(" ==> ");
		n->reg.colour = regmap[i][n->mode];
		locprint(stdout, n);
		printf("\n");
		found = 1;
		break;
	    }
	}
	if (!found) {
	    spilled = 1;
	    bsput(s->spilled, n->reg.id);
	}
    }
    for (l = 0; bsiter(s->coalesced, &l); l++) {
	n = locmap[getalias(s, l)];
	locmap[l]->reg.colour = n->reg.colour;
    }
    return spilled;
}

void rewrite(Isel *s)
{
    die("Rewrite spills!");
}

void regalloc(Isel *s)
{
    size_t i;
    int spilled;

    s->prepainted = mkbs();
    for (i = 0; i < maxregid; i++)
	if (locmap[i]->reg.colour)
	    bsput(s->prepainted, i);
    do {
	setup(s);
	liveness(s);
	build(s);
	mkworklist(s);
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
		selspill(s);
	} while (s->nwlsimp || s->nwlmove || s->nwlfreeze || s->nwlspill);
	spilled = paint(s);
	if (spilled)
	    rewrite(s);
    } while (spilled);

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
	    if (gbhasedge(s, i, j)) {
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

