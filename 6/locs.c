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
    l->mode = ModeQ;
    l->lbl = strdup(lbl);
    return l;
}

Loc *loclitl(char *lbl)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Loclitl;
    l->mode = ModeQ;
    l->lbl = strdup(lbl);
    return l;
}

Loc *loclbl(Node *e)
{
    Node *lbl;
    assert(e->type == Nexpr);
    lbl = e->expr.args[0];
    assert(lbl->type == Nlit);
    assert(lbl->lit.littype = Llbl);
    return locstrlbl(lbl->lit.lblval);
}

Loc **locmap = NULL;
size_t maxregid = 0;

static Loc *locregid(regid id, Mode m)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Locreg;
    l->mode = m;
    l->reg.id = id;
    locmap = xrealloc(locmap, maxregid * sizeof(Loc*));
    locmap[l->reg.id] = l;
    return l;
}

Loc *locreg(Mode m)
{
    return locregid(maxregid++, m);
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
    l->type = Locmeml;
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


Loc *loclit(long val, Mode m)
{
    Loc *l;

    l = zalloc(sizeof(Loc));
    l->type = Loclit;
    l->mode = m;
    l->lit = val;
    return l;
}

Loc *coreg(Reg r, Mode m)
{
    Reg crtab[][Nmode + 1] = {
        [Ral]  = {Rnone, Ral,  Rax,  Reax, Rrax},
        [Rcl]  = {Rnone, Rcl,  Rcx,  Recx, Rrcx},
        [Rdl]  = {Rnone, Rdl,  Rdx,  Redx, Rrdx},
        [Rbl]  = {Rnone, Rbl,  Rbx,  Rebx, Rrbx},
        [Rsil] = {Rnone, Rsil, Rsi,  Resi, Rrsi},
        [Rdil] = {Rnone, Rdil, Rdi,  Redi, Rrdi},
        [R8b]  = {Rnone, R8b,  R8w,  R8d,  R8},
        [R9b]  = {Rnone, R9b,  R9w,  R9d,  R9},
        [R10b] = {Rnone, R10b, R10w, R10d, R10},
        [R11b] = {Rnone, R11b, R11w, R11d, R11},
        [R12b] = {Rnone, R12b, R12w, R12d, R12},
        [R13b] = {Rnone, R13b, R13w, R13d, R13},
        [R14b] = {Rnone, R14b, R14w, R14d, R14},
        [R15b] = {Rnone, R15b, R15w, R15d, R15},

        [Rax]  = {Rnone, Ral,  Rax,  Reax, Rrax},
        [Rcx]  = {Rnone, Rcl,  Rcx,  Recx, Rrcx},
        [Rdx]  = {Rnone, Rdl,  Rdx,  Redx, Rrdx},
        [Rbx]  = {Rnone, Rbl,  Rbx,  Rebx, Rrbx},
        [Rsi]  = {Rnone, Rsil, Rsi,  Resi, Rrsi},
        [Rdi]  = {Rnone, Rsil, Rdi,  Redi, Rrdi},
        [R8w]  = {Rnone, R8b,  R8w,  R8d,  R8},
        [R9w]  = {Rnone, R9b,  R9w,  R9d,  R9},
        [R10w] = {Rnone, R10b, R10w, R10d, R10},
        [R11w] = {Rnone, R11b, R11w, R11d, R11},
        [R12w] = {Rnone, R12b, R12w, R12d, R12},
        [R13w] = {Rnone, R13b, R13w, R13d, R13},
        [R14w] = {Rnone, R14b, R14w, R14d, R14},
        [R15w] = {Rnone, R15b, R15w, R15d, R15},

        [Reax] = {Rnone, Ral,  Rax,  Reax, Rrax},
        [Recx] = {Rnone, Rcl,  Rcx,  Recx, Rrcx},
        [Redx] = {Rnone, Rdl,  Rdx,  Redx, Rrdx},
        [Rebx] = {Rnone, Rbl,  Rbx,  Rebx, Rrbx},
        [Resi] = {Rnone, Rsil, Rsi,  Resi, Rrsi},
        [Redi] = {Rnone, Rsil, Rdi,  Redi, Rrdi},
        [R8d]  = {Rnone, R8b,  R8w,  R8d,  R8},
        [R9d]  = {Rnone, R9b,  R9w,  R9d,  R9},
        [R10d] = {Rnone, R10b, R10w, R10d, R10},
        [R11d] = {Rnone, R11b, R11w, R11d, R11},
        [R12d] = {Rnone, R12b, R12w, R12d, R12},
        [R13d] = {Rnone, R13b, R13w, R13d, R13},
        [R14d] = {Rnone, R14b, R14w, R14d, R14},
        [R15d] = {Rnone, R15b, R15w, R15d, R15},

        [Rrax] = {Rnone, Ral,  Rax,  Reax, Rrax},
        [Rrcx] = {Rnone, Rcl,  Rcx,  Recx, Rrcx},
        [Rrdx] = {Rnone, Rdl,  Rdx,  Redx, Rrdx},
        [Rrbx] = {Rnone, Rbl,  Rbx,  Rebx, Rrbx},
        [Rrsi] = {Rnone, Rsil, Rsi,  Resi, Rrsi},
        [Rrdi] = {Rnone, Rsil, Rdi,  Redi, Rrdi},
        [R8]   = {Rnone, R8b,  R8w,  R8d,  R8},
        [R9]   = {Rnone, R9b,  R9w,  R9d,  R9},
        [R10]  = {Rnone, R10b, R10w, R10d, R10},
        [R11]  = {Rnone, R11b, R11w, R11d, R11},
        [R12]  = {Rnone, R12b, R12w, R12d, R12},
        [R13]  = {Rnone, R13b, R13w, R13d, R13},
        [R14]  = {Rnone, R14b, R14w, R14d, R14},
        [R15]  = {Rnone, R15b, R15w, R15d, R15},
    };

    assert(crtab[r][m] != Rnone);
    return locphysreg(crtab[r][m]);
}
