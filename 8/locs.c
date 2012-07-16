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

Mode regmodes[] = {
#define Reg(r, name, mode) mode,
#include "regs.def"
#undef Reg
};

char *regnames[] = {
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


char *genlblstr(char *buf, size_t sz)
{
    static int nextlbl;
    snprintf(buf, 128, ".L%d", nextlbl++);
    return buf;
}

Node *genlbl(void)
{
    char buf[128];

    genlblstr(buf, 128);
    return mklbl(-1, buf);
}

Loc *locstrlbl(char *lbl)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Loclbl;
    l->mode = ModeL;
    l->lbl = strdup(lbl);
    return l;
}

Loc *loclitl(char *lbl)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Loclitl;
    l->mode = ModeL;
    l->lbl = strdup(lbl);
    return l;
}

Loc *loclbl(Node *lbl)
{
    assert(lbl->type = Nlbl);
    return locstrlbl(lbl->lbl.name);
}

Loc **locmap = NULL;
size_t maxregid = 0;

Loc *locreg(Mode m)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Locreg;
    l->mode = m;
    l->reg.id = maxregid++;
    locmap = xrealloc(locmap, maxregid * sizeof(Loc*));
    locmap[l->reg.id] = l;
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
