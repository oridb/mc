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

typedef struct Usemap Usemap;
struct Usemap {
    int l[Maxarg + 1]; /* location of arg used in instruction's arg list */
    int r[Maxarg + 1]; /* list of registers used implicitly by instruction */
};

void wlprint(FILE *fd, char *name, Loc **wl, size_t nwl);
static int moverelated(Isel *s, regid n);
static void printedge(FILE *fd, char *msg, size_t a, size_t b);
static void check(Isel *s);

/* tables of uses/defs by instruction */
Usemap usetab[] = {
#define Use(...) {__VA_ARGS__}
#define Insn(i, fmt, use, def) use,
#include "insns.def"
#undef Insn
#undef Use
};

Usemap deftab[] = {
#define Def(...) {__VA_ARGS__}
#define Insn(i, fmt, use, def) def,
#include "insns.def"
#undef Insn
#undef Def
};

/* A map of which registers interfere */
Reg regmap[][Nmode] = {
    [0]  = {Rnone, Ral, Rax, Reax, Rrax},
    [1]  = {Rnone, Rcl, Rcx, Recx, Rrcx},
    [2]  = {Rnone, Rdl, Rdx, Redx, Rrdx},
    [3]  = {Rnone, Rbl, Rbx, Rebx, Rrbx},
    [4]  = {Rnone, Rsil, Rsi, Resi, Rrsi},
    [5]  = {Rnone, Rdil, Rdi, Redi, Rrdi},
    [6]  = {Rnone, Rr8b, Rr8w, Rr8d, Rr8},
    [7]  = {Rnone, Rr9b, Rr9w, Rr9d, Rr9},
    [8]  = {Rnone, Rr10b, Rr10w, Rr10d, Rr10},
    [9]  = {Rnone, Rr11b, Rr11w, Rr11d, Rr11},
    [10]  = {Rnone, Rr12b, Rr12w, Rr12d, Rr12},
    [11]  = {Rnone, Rr13b, Rr13w, Rr13d, Rr13},
    [12]  = {Rnone, Rr14b, Rr14w, Rr14d, Rr14},
    [13]  = {Rnone, Rr15b, Rr15w, Rr15d, Rr15},
};

/* Which regmap entry a register maps to */
int colourmap[Nreg] = {
    /* byte */
    [Ral] = 0,
    [Rcl] = 1,
    [Rdl] = 2,
    [Rbl] = 3,
    [Rsil] = 4,
    [Rdil] = 5,
    [Rr8b] = 6,
    [Rr9b] = 7,
    [Rr10b] = 8,
    [Rr11b] = 9,
    [Rr12b] = 10,
    [Rr13b] = 11,
    [Rr14b] = 12,
    [Rr15b] = 13,

    /* word */
    [Rax] = 0,
    [Rcx] = 1,
    [Rdx] = 2,
    [Rbx] = 3,
    [Rsi] = 4,
    [Rdi] = 5,
    [Rr8w] = 6,
    [Rr9w] = 7,
    [Rr10w] = 8,
    [Rr11w] = 9,
    [Rr12w] = 10,
    [Rr13w] = 11,
    [Rr14w] = 12,
    [Rr15w] = 13,

    /* dword */
    [Reax] = 0,
    [Recx] = 1,
    [Redx] = 2,
    [Rebx] = 3,
    [Resi] = 4,
    [Redi] = 5,
    [Rr8d] = 6,
    [Rr9d] = 7,
    [Rr10d] = 8,
    [Rr11d] = 9,
    [Rr12d] = 10,
    [Rr13d] = 11,
    [Rr14d] = 12,
    [Rr15d] = 13,

    /* qword */
    [Rrax] = 0,
    [Rrcx] = 1,
    [Rrdx] = 2,
    [Rrbx] = 3,
    [Rrsi] = 4,
    [Rrdi] = 5,
    [Rr8] = 6,
    [Rr9] = 7,
    [Rr10] = 8,
    [Rr11] = 9,
    [Rr12] = 10,
    [Rr13] = 11,
    [Rr14] = 12,
    [Rr15] = 13,
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

static size_t uses(Insn *insn, regid *u)
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
        if (m->mem.base)
            if (!isfixreg(m->mem.base))
                u[j++] = m->mem.base->reg.id;
        if (m->mem.idx)
            if (!isfixreg(m->mem.base))
                u[j++] = m->mem.idx->reg.id;
    }
    return j;
}

static size_t defs(Insn *insn, regid *d)
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

/* The uses and defs for an entire BB. */
static void udcalc(Asmbb *bb)
{
    /* up to 2 registers per memloc, so
     * 2*Maxarg is the maximum number of
     * uses or defs we can see */
    regid   u[Maxuse], d[Maxdef];
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

static int gbhasedge(Isel *s, size_t u, size_t v)
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
    s->gbits[i/Sizetbits] |= 1ULL << (i % Sizetbits);
    s->gbits[j/Sizetbits] |= 1ULL << (j % Sizetbits);
    assert(gbhasedge(s, u, v) && gbhasedge(s, v, u));
}

static int wlhas(Loc **wl, size_t nwl, regid v, size_t *idx)
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

/*
 * If we have an edge between two aliasing registers,
 * we should not increment the degree, since that would
 * be double counting.
 */
static int degreechange(Isel *s, regid u, regid v)
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

static void addedge(Isel *s, regid u, regid v)
{
    if (u == v || gbhasedge(s, u, v))
        return;
    if (u == Rrbp || u == Rrsp || u == Rrip)
        return;
    if (v == Rrbp || v == Rrsp || v == Rrip)
        return;

    gbputedge(s, u, v);
    gbputedge(s, v, u);
    if (!bshas(s->prepainted, u)) {
        bsput(s->gadj[u], v);
        s->degree[u] += degreechange(s, v, u);
    }
    if (!bshas(s->prepainted, v)) {
        bsput(s->gadj[v], u);
        s->degree[v] += degreechange(s, u, v);
    }
}

static void setup(Isel *s)
{
    size_t gchunks;
    size_t i;

    free(s->gbits);
    gchunks = (maxregid*maxregid)/Sizetbits + 1;
    s->gbits = zalloc(gchunks*sizeof(size_t));
    /* fresh adj list repr. */
    free(s->gadj);
    s->gadj = zalloc(maxregid * sizeof(Bitset*));
    for (i = 0; i < maxregid; i++)
        s->gadj[i] = mkbs();

    s->spilled = bsclear(s->spilled);
    s->coalesced = bsclear(s->coalesced);
    lfree(&s->wlspill, &s->nwlspill);
    lfree(&s->wlfreeze, &s->nwlfreeze);
    lfree(&s->wlsimp, &s->nwlsimp);

    free(s->aliasmap);
    free(s->degree);
    free(s->rmoves);
    free(s->nrmoves);

    s->aliasmap = zalloc(maxregid * sizeof(size_t));
    s->degree = zalloc(maxregid * sizeof(int));
    s->rmoves = zalloc(maxregid * sizeof(Loc **));
    s->nrmoves = zalloc(maxregid * sizeof(size_t));

    for (i = 0; bsiter(s->prepainted, &i); i++)
        s->degree[i] = 1<<16;
}

static void build(Isel *s)
{
    regid u[Maxuse], d[Maxdef];
    size_t nu, nd;
    size_t i, k, a;
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

            /* add these to the initial set */
            for (k = 0; k < nu; k++)
                bsput(s->initial, u[k]);
            for (k = 0; k < nd; k++)
                bsput(s->initial, d[k]);

            /* moves get special treatment, since we don't want spurious
             * edges between the src and dest */
            //iprintf(stdout, insn);
            if (ismove(insn)) {
                /* live \= uses(i) */
                for (k = 0; k < nu; k++) {
                    /* remove all physical register aliases */
                    if (bshas(s->prepainted, u[k])) {
                        for (a = 0; a < Nmode; a++)
                            bsdel(live, regmap[colourmap[u[k]]][a]);
                    } else {
                        bsdel(live, u[k]);
                    }
                }

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

static int adjiter(Isel *s, regid n, regid *m)
{
    size_t i, r;

    for (r = *m; bsiter(s->gadj[n], &r); r++) {
        for (i = 0; i < s->nselstk; i++)
            if (r == s->selstk[i]->reg.id)
                goto next;
        if (bshas(s->coalesced, r))
            goto next;
        *m = r;
        return 1;
next:
        continue;
    }
    return 0;
}

static size_t nodemoves(Isel *s, regid n, Insn ***pil)
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
                else
                    count++;
            }
        }
        for (j = 0; j < s->nwlmove; j++) {
            if (s->wlmove[j] == s->rmoves[n][i]) {
                if (pil)
                    lappend(pil, &count, s->rmoves[n][i]);
                else
                    count++;
            }
        }
    }
    return count;
}

static int moverelated(Isel *s, regid n)
{
    return nodemoves(s, n, NULL) != 0;
}

static void mkworklist(Isel *s)
{
    size_t i;

    for (i = 0; bsiter(s->initial, &i); i++) {
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

static void enablemove(Isel *s, regid n)
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

static void decdegree(Isel *s, regid m)
{
    int d;
    size_t idx;
    regid n;

    assert(m < maxregid);
    d = s->degree[m];
    s->degree[m]--;

    if (d == K) {
        enablemove(s, m);
        for (n = 0; adjiter(s, m, &n); n++)
            enablemove(s, n);
        if (!wlhas(s->wlspill, s->nwlspill, m, &idx))
            die("%zd not in wlspill", m);
        ldel(&s->wlspill, &s->nwlspill, idx);
        if (moverelated(s, m)) {
            lappend(&s->wlfreeze, &s->nwlfreeze, locmap[m]);
        } else {
            lappend(&s->wlsimp, &s->nwlsimp, locmap[m]);
        }
    }
}

static void simp(Isel *s)
{
    Loc *l;
    regid m;

    check(s);
    l = lpop(&s->wlsimp, &s->nwlsimp);
    lappend(&s->selstk, &s->nselstk, l);
    for (m = 0; adjiter(s, l->reg.id, &m); m++) {
        decdegree(s, m);
    }
    check(s);
}

static regid getalias(Isel *s, regid id)
{
    while (1) {
        if (!s->aliasmap[id])
            break;
        id = s->aliasmap[id]->reg.id;
    };
    return id;
}

void whichwl(Isel *s, regid u)
{
    size_t x;

    if (wlhas(s->wlsimp, s->nwlsimp, u, &x)) printf("%zd on simp\n", u);
    if (wlhas(s->wlfreeze, s->nwlfreeze, u, &x)) printf("%zd on freeze\n", u);
    if (wlhas(s->wlspill, s->nwlspill, u, &x)) printf("%zd on spill\n", u);
    if (wlhas(s->selstk, s->nselstk, u, &x)) printf("%zd on select stack\n", u);
    if (bshas(s->coalesced, u)) printf("%zd on coalesced\n", u);
    if (bshas(s->spilled, u)) printf("%zd on stack\n", u);
}

static void wladd(Isel *s, regid u)
{
    size_t i;

    if (bshas(s->prepainted, u))
        return;
    if (moverelated(s, u))
        return;
    if (s->degree[u] >= K)
        return;

    check(s);
    assert(wlhas(s->wlfreeze, s->nwlfreeze, u, &i));
    ldel(&s->wlfreeze, &s->nwlfreeze, i);
    lappend(&s->wlsimp, &s->nwlsimp, locmap[u]);
    check(s);
}

static int conservative(Isel *s, regid u, regid v)
{
    int k;
    regid n;

    k = 0;
    for (n = 0; adjiter(s, u, &n); n++)
        if (s->degree[n] >= K)
            k++;
    for (n = 0; adjiter(s, v, &n); n++)
        if (s->degree[n] >= K)
            k++;
    return k < K;
}

/* FIXME: is this actually correct? */
static int ok(Isel *s, regid t, regid r)
{
    if (s->degree[t] < K)
        return 1;
    if (bshas(s->prepainted, t))
        return 1;
    if (gbhasedge(s, t, r))
        return 1;
    return 0;
}

static int combinable(Isel *s, regid u, regid v)
{
    regid t;

    if (debugopt['r'])
        printf("check combinable %zd <=> %zd\n", u, v);
    /* Regs of different modes can't be combined as things stand.
     * In principle they should be combinable, but it confused the
     * whole mode dance. */
    if (locmap[u]->mode != locmap[v]->mode)
        return 0;
    /* if u isn't prepainted, can we conservatively coalesce? */
    if (!bshas(s->prepainted, u) && conservative(s, u, v))
        return 1;

    /* if it is, are the adjacent nodes ok to combine with this? */
    for (t = 0; adjiter(s, v, &t); t++)
        if (!ok(s, t, u))
            return 0;
    return 1;
}

static void combine(Isel *s, regid u, regid v)
{
    regid t;
    size_t idx;
    size_t i, j;
    int has;

    if (debugopt['r'])
        printedge(stdout, "combining:", u, v);
    if (wlhas(s->wlfreeze, s->nwlfreeze, v, &idx))
        ldel(&s->wlfreeze, &s->nwlfreeze, idx);
    else if (wlhas(s->wlspill, s->nwlspill, v, &idx)) {
        ldel(&s->wlspill, &s->nwlspill, idx);
    }
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

    for (t = 0; adjiter(s, v, &t); t++) {
        if (debugopt['r'])
            printedge(stdout, "combine-putedge:", v, t);
        addedge(s, t, u);
        decdegree(s, t);
    }
    if (s->degree[u] >= K && wlhas(s->wlfreeze, s->nwlfreeze, u, &idx)) {
        ldel(&s->wlfreeze, &s->nwlfreeze, idx);
        lappend(&s->wlspill, &s->nwlspill, locmap[u]);
    }
}

static void coalesce(Isel *s)
{
    Insn *m;
    regid u, v, tmp;

    check(s);
    m = lpop(&s->wlmove, &s->nwlmove);
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
        check(s);
        wladd(s, u);
        check(s);
    } else {
        lappend(&s->mactive, &s->nmactive, m);
    }
}

static int mldel(Insn ***ml, size_t *nml, Insn *m)
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

static void freezemoves(Isel *s, Loc *u)
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

        if (!mldel(&s->mactive, &s->nmactive, m))
            mldel(&s->wlmove, &s->nwlmove, m);
        lappend(&s->mfrozen, &s->nmfrozen, m);
        check(s);
        if (!nodemoves(s, v->reg.id, NULL) && s->degree[v->reg.id] < K) {
            if (!wlhas(s->wlfreeze, s->nwlfreeze, v->reg.id, &idx))
                die("Reg %zd not in freeze wl\n", v->reg.id);
            ldel(&s->wlfreeze, &s->nwlfreeze, idx);
            lappend(&s->wlsimp, &s->nwlsimp, v);
        }
        check(s);

    }
    lfree(&ml, &nml);
}

static void freeze(Isel *s)
{
    Loc *l;

    check(s);
    l = lpop(&s->wlfreeze, &s->nwlfreeze);
    lappend(&s->wlsimp, &s->nwlsimp, l);
    freezemoves(s, l);
    check(s);
}

/* Select the spill candidates */
static void selspill(Isel *s)
{
    size_t i;
    Loc *m;

    check(s);
    /* FIXME: pick a better heuristic for spilling */
    m = NULL;
    for (i = 0; i < s->nwlspill; i++) {
        if (!bshas(s->shouldspill, s->wlspill[i]->reg.id))
            continue;
        m = s->wlspill[i];
        ldel(&s->wlspill, &s->nwlspill, i);
        break;
    }
    if (!m) {
        for (i = 0; i < s->nwlspill; i++) {
            if (bshas(s->neverspill, s->wlspill[i]->reg.id)) {
                printf("Not spilling %zd\n", s->wlspill[i]->reg.id);
                continue;
            }
            m = s->wlspill[i];
            ldel(&s->wlspill, &s->nwlspill, i);
            break;
        }
    }
    assert(m != NULL);
    lappend(&s->wlsimp, &s->nwlsimp, m);
    freezemoves(s, m);
    check(s);
}

/*
 * Selects the colors for registers, spilling to the
 * stack if no free registers can be found.
 */
static int paint(Isel *s)
{
    int taken[K + 2]; /* esp, ebp aren't "real colours" */
    Loc *n, *w;
    regid l;
    size_t i;
    int spilled;
    int found;

    spilled = 0;
    while (s->nselstk) {
        bzero(taken, K*sizeof(int));
        n = lpop(&s->selstk, &s->nselstk);

        for (l = 0; bsiter(s->gadj[n->reg.id], &l); l++) {
            if (debugopt['r'] > 1)
                printedge(stdout, "paint-edge:", n->reg.id, l);
            w = locmap[getalias(s, l)];
            if (w->reg.colour)
                taken[colourmap[w->reg.colour]] = 1;
        }

        found = 0;
        for (i = 0; i < K; i++) {
            if (!taken[i]) {
                if (debugopt['r']) {
                    fprintf(stdout, "\tselecting ");
                    locprint(stdout, n, 'x');
                    fprintf(stdout, " = %s\n", regnames[regmap[i][n->mode]]);
                }
                n->reg.colour = regmap[i][n->mode];
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

typedef struct Remapping Remapping;
struct Remapping {
    regid oldreg;
    Loc *newreg;
};

static Loc *mapfind(Loc *old, Remapping *r, size_t nr)
{
    Loc *new;
    Loc *base;
    Loc *idx;
    size_t i;

    if (!old)
        return NULL;

    new = NULL;
    if (old->type == Locreg) {
        for (i = 0; i < nr; i++) {
            if (old->reg.id == r[i].oldreg) {
                return r[i].newreg;
            }
        }
    } else if (old->type == Locmem || old->type == Locmeml) {
        base = mapfind(old->mem.base, r, nr);
        idx = mapfind(old->mem.idx, r, nr);
        if (base != old->mem.base || idx != old->mem.idx) {
            if (old->type == Locmem)
                new = locmems(old->mem.constdisp, base, idx, old->mem.scale, old->mode);
            else
                new = locmemls(old->mem.lbldisp, base, idx, old->mem.scale, old->mode);
        }
        if (new)
            return new;
    }
    return old;
}

static Loc *spillslot(Isel *s, regid reg)
{
    size_t stkoff;

    stkoff = (size_t)htget(s->spillslots, (void*)reg);
    return locmem(-stkoff, locphysreg(Rrbp), NULL, locmap[reg]->mode);
}

static void updatelocs(Isel *s, Insn *insn, Remapping *use, size_t nuse, Remapping *def, size_t ndef)
{
    size_t i;

    for (i = 0; i < insn->nargs; i++) {
        insn->args[i] = mapfind(insn->args[i], use, nuse);
        insn->args[i] = mapfind(insn->args[i], def, ndef);
    }
}

/*
 * Takes two tables for remappings, of size Maxuse/Maxdef,
 * and fills them, storign the number of uses or defs. Returns
 * whether there are any remappings at all.
 */
static int remap(Isel *s, Insn *insn, Remapping *use, size_t *nuse, Remapping *def, size_t *ndef)
{
    regid u[Maxuse], d[Maxdef];
    size_t nu, nd;
    size_t useidx, defidx;
    size_t i, j;
    int found;

    useidx = 0;
    nu = uses(insn, u);
    nd = defs(insn, d);
    for (i = 0; i < nu; i++) {
        if (!bshas(s->spilled, u[i]))
            continue;
        use[useidx].oldreg = u[i];
        use[useidx].newreg = locreg(locmap[u[i]]->mode);
        bsput(s->neverspill, use[useidx].newreg->reg.id);
        useidx++;
    }

    defidx = 0; 
    for (i = 0; i < nd; i++) {
        if (!bshas(s->spilled, d[i]))
            continue;
        def[defidx].oldreg = d[i];

        /* if we already have remapped a use for this register, we want to
         * store the same register from the def. */
        found = 0;
        for (j = 0; j < defidx; j++) {
            if (def[i].oldreg == d[i]) {
                def[defidx].newreg = locreg(locmap[d[i]]->mode);
                bsput(s->neverspill, def[defidx].newreg->reg.id);
                found = 1;
            }
        }
        if (!found) {
            def[defidx].newreg = locreg(locmap[d[i]]->mode);
            bsput(s->neverspill, def[defidx].newreg->reg.id);
        }

        defidx++;
    }

    *nuse = useidx;
    *ndef = defidx;
    return useidx > 0 || defidx > 0;
}

/*
 * Rewrite instructions using spilled registers, inserting
 * appropriate loads and stores into the BB
 */
static void rewritebb(Isel *s, Asmbb *bb)
{
    Remapping use[Maxuse], def[Maxdef];
    Insn *insn;
    size_t nuse, ndef;
    size_t i, j;
    Insn **new;
    size_t nnew;

    new = NULL;
    nnew = 0;
    for (j = 0; j < bb->ni; j++) {
        insn = bb->il[j];
        if (ismove(insn)) {
            if (getalias(s, insn->args[0]->reg.id) == getalias(s, insn->args[1]->reg.id))
                continue;
        }
        /* if there is a remapping, insert the loads and stores as needed */
        if (remap(s, bb->il[j], use, &nuse, def, &ndef)) {
            for (i = 0; i < nuse; i++) {
                insn = mkinsn(Imov, spillslot(s, use[i].oldreg), use[i].newreg, NULL);
                lappend(&new, &nnew, insn);
                if (debugopt['r']) {
                    printf("loading ");
                    locprint(stdout, locmap[use[i].oldreg], 'x');
                    printf(" -> ");
                    locprint(stdout, use[i].newreg, 'x');
                    printf("\n");
                }
            }
            insn = bb->il[j];
            updatelocs(s, insn, use, nuse, def, ndef);
            lappend(&new, &nnew, insn);
            for (i = 0; i < ndef; i++) {
                insn = mkinsn(Imov, def[i].newreg, spillslot(s, def[i].oldreg), NULL);
                lappend(&new, &nnew, insn);
                if (debugopt['r']) {
                    printf("storing ");
                    locprint(stdout, locmap[def[i].oldreg], 'x');
                    printf(" -> ");
                    locprint(stdout, def[i].newreg, 'x');
                    printf("\n");
                }
            }
        } else {
            lappend(&new, &nnew, bb->il[j]);
        }
    }
    lfree(&bb->il, &bb->ni);
    bb->il = new;
    bb->ni = nnew;
}

static void addspill(Isel *s, Loc *l)
{
    s->stksz->lit += modesize[l->mode];
    s->stksz->lit = align(s->stksz->lit, modesize[l->mode]);
    if (debugopt['r']) {
        printf("spill ");
        locprint(stdout, l, 'x');
        printf(" to %zd(%%rbp)\n", s->stksz->lit);
    }
    htput(s->spillslots, (void *)l->reg.id, (void *)s->stksz->lit);
}

/* 
 * Rewrites the function code so that it no longer contains
 * references to spilled registers. Every use of spilled regs
 *
 *      insn %rX,%rY
 *
 * is rewritten to look like:
 *
 *      mov 123(%rsp),%rZ
 *      insn %rZ,%rW
 *      mov %rW,234(%rsp)
 */
static void rewrite(Isel *s)
{
    size_t i;

    s->spillslots = mkht(ptrhash, ptreq);
    /* set up stack locations for all spilled registers. */
    for (i = 0; bsiter(s->spilled, &i); i++)
        addspill(s, locmap[i]);

    /* rewrite instructions using them */
    for (i = 0; i < s->nbb; i++)
        rewritebb(s, s->bb[i]);
    htfree(s->spillslots);
    bsclear(s->spilled);
}

void regalloc(Isel *s)
{
    int spilled;
    size_t i;

    /* Initialize the list of prepainted registers */
    s->prepainted = mkbs();
    bsput(s->prepainted, 0);
    for (i = 0; i < Nreg; i++)
        bsput(s->prepainted, i);

    s->shouldspill = mkbs();
    s->neverspill = mkbs();
    s->initial = mkbs();
    for (i = 0; i < Nsaved; i++)
        bsput(s->shouldspill, s->calleesave[i]->reg.id);
    spilled = 0;
    do {
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
            else if (s->nwlspill)
                selspill(s);
        } while (s->nwlsimp || s->nwlmove || s->nwlfreeze || s->nwlspill);
        spilled = paint(s);
        if (spilled)
            rewrite(s);
    } while (spilled);
    bsfree(s->prepainted);
    bsfree(s->shouldspill);
    bsfree(s->neverspill);
}

void wlprint(FILE *fd, char *name, Loc **wl, size_t nwl)
{
    size_t i;
    char *sep;

    sep = "";
    fprintf(fd, "%s = [", name);
    for (i = 0; i < nwl; i++) {
        fprintf(fd, "%s", sep);
        locprint(fd, wl[i], 'x');
        fprintf(fd, "(%zd)", wl[i]->reg.id);
        sep = ",";
    }
    fprintf(fd, "]\n");
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
            locprint(fd, locmap[i], 'x');
            sep = ",";
        }
    }
    fprintf(fd, "\n");
}

static void printedge(FILE *fd, char *msg, size_t a, size_t b)
{
    fprintf(fd, "\t%s ", msg);
    locprint(fd, locmap[a], 'x');
    fprintf(fd, " -- ");
    locprint(fd, locmap[b], 'x');
    fprintf(fd, "\n");
}

void dumpjustasm(Isel *s)
{
    size_t i, j;
    Asmbb *bb;

    for (j = 0; j < s->nbb; j++) {
        bb = s->bb[j];
        for (i = 0; i < bb->ni; i++)
            iprintf(stdout, bb->il[i]);
    }
}

void dumpasm(Isel *s, FILE *fd)
{
    size_t i, j;
    char *sep;
    Asmbb *bb;

    fprintf(fd, "WORKLISTS -- \n");
    wlprint(stdout, "spill", s->wlspill, s->nwlspill);
    wlprint(stdout, "simp", s->wlsimp, s->nwlsimp);
    wlprint(stdout, "freeze", s->wlfreeze, s->nwlfreeze);
    /* noisy to dump this all the time; only dump for higher debug levels */
    if (debugopt['r'] > 2) {
        fprintf(fd, "IGRAPH ----- \n");
        for (i = 0; i < maxregid; i++) {
            for (j = i; j < maxregid; j++) {
                if (gbhasedge(s, i, j))
                    printedge(stdout, "", i, j);
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

static int findmove(Isel *s, Insn *m)
{
    size_t i;
    for (i = 0; i < s->nmactive; i++)
        if (s->mactive[i] == m)
            return 1;
    for (i = 0; i < s->nwlmove; i++)
        if (s->wlmove[i] == m)
            return 1;
    return 0;
}

static void check(Isel *s)
{
    size_t i, j;
    size_t n;
    size_t idx;
    char foo[5];

    for (i = 0; bsiter(s->initial, &i); i++) {
        /* check worklists are disjoint */
        n = 0;
        if (bshas(s->prepainted, i)) {
            foo[n] = 'p';
            n++;
        }
        if (wlhas(s->wlsimp, s->nwlsimp, i, &idx)) {
            foo[n] = 's';
            /* check simplify invariant */
            for (j = 0; j < s->nrmoves[j]; j++)
                assert("simp invariant" && !findmove(s, s->rmoves[i][j]));
            n++;
        }
        if (wlhas(s->wlfreeze, s->nwlfreeze, i, &idx)) {
            foo[n] = 'f';
            /* check freeze invariant */
            for (j = 0; j < s->nrmoves[j]; j++)
                assert("freeze invariant" && findmove(s, s->rmoves[i][j]));
            n++;
        }
        if (wlhas(s->wlspill, s->nwlspill, i, &idx)) {
            foo[n] = 'd';
            n++;
        }
        if (wlhas(s->selstk, s->nselstk, i, &idx)) {
            foo[n] = 't';
            n++;
        }
        if (bshas(s->coalesced, i)) {
            foo[n] = 'k';
            n++;
        }
        if (bshas(s->spilled, i)) {
            foo[n] = 'l';
            n++;
        }
        foo[n]='\0';
        if (n != 1)
            printf("%s\n", foo);
        assert(n == 1);
    }
}

void edges(Isel *s, regid u)
{
    size_t i;

    for (i = 0; i < maxregid; i++) {
        if (gbhasedge(s, u, i))
            printf("%zd ", i);
    }
    printf("\n");
}

