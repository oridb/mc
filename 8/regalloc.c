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
#include "asm.h"

const Mode regmodes[] = {
#define Reg(r, name, mode) mode,
#include "regs.def"
#undef Reg
};

const char *regnames[] = {
#define Reg(r, name, mode) name,
#include "regs.def"
#undef Reg
};


const Reg reginterferes[Nreg][Nmode + 1] = {
    /* byte */
    [Ral] = {Ral, Rax, Reax},
    [Rcl] = {Rcl, Rcx, Recx},
    [Rdl] = {Rdl, Rdx, Redx},
    [Rbl] = {Rbl, Rbx, Rebx},

    /* word */
    [Rax] = {Ral, Rax, Reax},
    [Rcx] = {Rcl, Rcx, Recx},
    [Rdx] = {Rdl, Rdx, Redx},
    [Rbx] = {Rbl, Rbx, Rebx},
    [Rsi] = {Rsi, Resi},
    [Rdi] = {Rdi, Redi},

    /* dword */
    [Reax] = {Ral, Rax, Reax},
    [Recx] = {Rcl, Rcx, Recx},
    [Redx] = {Rdl, Rdx, Redx},
    [Rebx] = {Rbl, Rbx, Rebx},
    [Resi] = {Rsi, Resi},
    [Redi] = {Rdi, Redi},
    [Resp] = {Resp},
    [Rebp] = {Rebp},
};

Loc *locstrlbl(Loc *l, char *lbl)
{
    l->type = Loclbl;
    l->mode = ModeL;
    l->lbl = strdup(lbl);
    return l;
}

Loc *loclbl(Loc *l, Node *lbl)
{
    assert(lbl->type = Nlbl);
    return locstrlbl(l, lbl->lbl.name);
}

Loc *locreg(Loc *l, Reg r)
{
    l->type = Locreg;
    l->mode = regmodes[r];
    l->reg = r;
    return l;
}

Loc *locmem(Loc *l, long disp, Reg base, Reg idx, Mode mode)
{
    l->type = Locmem;
    l->mode = mode;
    l->mem.constdisp = disp;
    l->mem.base = base;
    l->mem.idx = idx;
    l->mem.scale = 0;
    return l;
}

Loc *locmems(Loc *l, long disp, Reg base, Reg idx, int scale, Mode mode)
{
    locmem(l, disp, base, idx, mode);
    l->mem.scale = scale;
    return l;
}

Loc *locmeml(Loc *l, char *disp, Reg base, Reg idx, Mode mode)
{
    l->type = Locmem;
    l->mode = mode;
    l->mem.lbldisp = strdup(disp);
    l->mem.base = base;
    l->mem.idx = idx;
    l->mem.scale = 0;
    return l;
}

Loc *locmemls(Loc *l, char *disp, Reg base, Reg idx, int scale, Mode mode)
{
    locmeml(l, disp, base, idx, mode);
    l->mem.scale = scale;
    return l;
}


Loc *loclit(Loc *l, long val)
{
    l->type = Loclit;
    l->mode = ModeL; /* FIXME: what do we do for mode? */
    l->lit = val;
    return l;
}

Loc getreg(Isel *s, Mode m)
{

    Loc l;
    int i;

    assert(m != ModeNone);
    l.reg = Rnone;
    for (i = 0; i < Nreg; i++) {
        if (!s->rtaken[i] && regmodes[i] == m) {
            locreg(&l, i);
            break;
        }
    }
    if (l.reg == Rnone)
        die("Not enough registers. Please split your expression and try again (FIXME: implement spilling)");
    for (i = 0; i < Nmode; i++)
        s->rtaken[reginterferes[l.reg][i]] = 1;

    return l;
}

Loc claimreg(Isel *s, Reg r)
{
    Loc l;
    int i;

    if (s->rtaken[r])
        die("Reg %s is already taken", regnames[r]);
    for (i = 0; i < Nmode; i++)
        s->rtaken[reginterferes[r][i]] = 1;
    locreg(&l, r);
    return l;
}

void freereg(Isel *s, Reg r)
{
    int i;

    for (i = 0; i < Nmode; i++)
        s->rtaken[reginterferes[r][i]] = 0;
}

void freeloc(Isel *s, Loc l)
{
    if (l.type == Locreg)
        freereg(s, l.reg);
}
