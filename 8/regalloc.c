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

Loc *locstrlbl(char *lbl)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Loclbl;
    l->mode = ModeL;
    l->lbl = strdup(lbl);
    return l;
}

Loc *loclbl(Node *lbl)
{
    assert(lbl->type = Nlbl);
    return locstrlbl(lbl->lbl.name);
}

Loc *locreg(Mode m)
{
    Loc *l;
    static long nextpseudo = 0;

    l = zalloc(sizeof(Loc));
    l->type = Locreg;
    l->mode = m;
    l->reg.pseudo = nextpseudo++;
    return l;
}

Loc *locphysreg(Reg r)
{
    static Loc *physregs[Nreg] = {0,};

    if (physregs[r])
        return physregs[r];
    physregs[r] = locreg(regmodes[r]);
    physregs[r]->reg.colour = r;
    return physregs[r];
}

Loc *locmem(long disp, Loc *base, Loc *idx, Mode mode)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Locmem;
    l->mode = mode;
    l->mem.constdisp = disp;
    l->mem.base = base;
    l->mem.idx = idx;
    l->mem.scale = 0;
    return l;
}

Loc *locmems(long disp, Loc *base, Loc *idx, int scale, Mode mode)
{
    Loc *l;

    l = locmem(disp, base, idx, mode);
    l->mem.scale = scale;
    return l;
}

Loc *locmeml(char *disp, Loc *base, Loc *idx, Mode mode)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Locmem;
    l->mode = mode;
    l->mem.lbldisp = strdup(disp);
    l->mem.base = base;
    l->mem.idx = idx;
    l->mem.scale = 0;
    return l;
}

Loc *locmemls(char *disp, Loc *base, Loc *idx, int scale, Mode mode)
{
    Loc *l;

    l = locmeml(disp, base, idx, mode);
    l->mem.scale = scale;
    return l;
}


Loc *loclit(long val)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Loclit;
    l->mode = ModeL; /* FIXME: what do we do for mode? */
    l->lit = val;
    return l;
}
