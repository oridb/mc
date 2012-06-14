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
	k = usetab[insn->op].l[i];
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
	    break;
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

    /* Add all the registers dsed and defined. Ddplicates
     * in this list are fine, since they're being added to
     * a set anyways */
    for (i = 0; i < Maxarg; i++) {
	if (!deftab[insn->op].l[i])
	    break;
	k = deftab[insn->op].l[i];
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

void bbliveness(Asmbb *bb)
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
	nd = uses(bb->il[i], d);
	for (j = 0; j < nu; j++)
	    if (!bshas(bb->def, u[j]))
		bsput(bb->use, u[j]);
	for (j = 0; j < nd; j++)
	    bsput(bb->def, d[j]);
    }
}

void liveness(Isel *s)
{
    Cfg *cfg;
    ssize_t i;
    int changed;

    cfg = s->cfg;
    cfg = cfg; /* shut up GCC for now */
    for (i = s->nbb - 1; i >= 0; i--)
	bbliveness(s->bb[i]);

    changed = 1;
    while (changed) {
	for (i = s->nbb - 1; i >= 0; i--) {
	}
	changed = 0;
    }
}

void regalloc(Isel *s)
{
    liveness(s);
}
