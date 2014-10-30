#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"
#include "mi.h"
#include "asm.h"
#include "../config.h"

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

size_t modesize[Nmode] = {
    [ModeNone]  = 0,
    [ModeB]     = 1,
    [ModeW]     = 2,
    [ModeL]     = 4,
    [ModeQ]     = 8,
    [ModeF]     = 4,
    [ModeD]     = 8,
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

int isintmode(Mode m)
{
    return m == ModeB || m == ModeW || m == ModeL || m == ModeQ;
}

int isfloatmode(Mode m)
{
    return m == ModeF || m == ModeD;
}

/* For x86, the assembly names are generated as follows:
 *      local symbols: .name
 *      un-namespaced symbols: <symprefix>name
 *      namespaced symbols: <symprefix>namespace$name
 */
char *asmname(Node *n)
{
    char *s;
    int len;

    len = strlen(Symprefix);
    if (n->name.ns)
        len += strlen(n->name.ns) + 1; /* +1 for separator */
    len += strlen(n->name.name) + 1;

    s = xalloc(len + 1);
    s[0] = '\0';
    if (n->name.ns)
        snprintf(s, len, "%s%s$%s", Symprefix, n->name.ns, n->name.name);
    else if (n->name.name[0] == '.')
        snprintf(s, len, "%s", n->name.name);
    else
        snprintf(s, len, "%s%s", Symprefix, n->name.name);
    return s;
}

char *genlblstr(char *buf, size_t sz)
{
    static int nextlbl;
    snprintf(buf, 128, ".L%d", nextlbl++);
    return buf;
}

Node *genlbl(Srcloc loc)
{
    char buf[128];

    genlblstr(buf, 128);
    return mklbl(loc, buf);
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
        [Rr8b]  = {Rnone, Rr8b,  Rr8w,  Rr8d,  Rr8},
        [Rr9b]  = {Rnone, Rr9b,  Rr9w,  Rr9d,  Rr9},
        [Rr10b] = {Rnone, Rr10b, Rr10w, Rr10d, Rr10},
        [Rr11b] = {Rnone, Rr11b, Rr11w, Rr11d, Rr11},
        [Rr12b] = {Rnone, Rr12b, Rr12w, Rr12d, Rr12},
        [Rr13b] = {Rnone, Rr13b, Rr13w, Rr13d, Rr13},
        [Rr14b] = {Rnone, Rr14b, Rr14w, Rr14d, Rr14},
        [Rr15b] = {Rnone, Rr15b, Rr15w, Rr15d, Rr15},

        [Rax]  = {Rnone, Ral,  Rax,  Reax, Rrax},
        [Rcx]  = {Rnone, Rcl,  Rcx,  Recx, Rrcx},
        [Rdx]  = {Rnone, Rdl,  Rdx,  Redx, Rrdx},
        [Rbx]  = {Rnone, Rbl,  Rbx,  Rebx, Rrbx},
        [Rsi]  = {Rnone, Rsil, Rsi,  Resi, Rrsi},
        [Rdi]  = {Rnone, Rsil, Rdi,  Redi, Rrdi},
        [Rr8w]  = {Rnone, Rr8b,  Rr8w,  Rr8d,  Rr8},
        [Rr9w]  = {Rnone, Rr9b,  Rr9w,  Rr9d,  Rr9},
        [Rr10w] = {Rnone, Rr10b, Rr10w, Rr10d, Rr10},
        [Rr11w] = {Rnone, Rr11b, Rr11w, Rr11d, Rr11},
        [Rr12w] = {Rnone, Rr12b, Rr12w, Rr12d, Rr12},
        [Rr13w] = {Rnone, Rr13b, Rr13w, Rr13d, Rr13},
        [Rr14w] = {Rnone, Rr14b, Rr14w, Rr14d, Rr14},
        [Rr15w] = {Rnone, Rr15b, Rr15w, Rr15d, Rr15},

        [Reax] = {Rnone, Ral,  Rax,  Reax, Rrax},
        [Recx] = {Rnone, Rcl,  Rcx,  Recx, Rrcx},
        [Redx] = {Rnone, Rdl,  Rdx,  Redx, Rrdx},
        [Rebx] = {Rnone, Rbl,  Rbx,  Rebx, Rrbx},
        [Resi] = {Rnone, Rsil, Rsi,  Resi, Rrsi},
        [Redi] = {Rnone, Rsil, Rdi,  Redi, Rrdi},
        [Rr8d]  = {Rnone, Rr8b,  Rr8w,  Rr8d,  Rr8},
        [Rr9d]  = {Rnone, Rr9b,  Rr9w,  Rr9d,  Rr9},
        [Rr10d] = {Rnone, Rr10b, Rr10w, Rr10d, Rr10},
        [Rr11d] = {Rnone, Rr11b, Rr11w, Rr11d, Rr11},
        [Rr12d] = {Rnone, Rr12b, Rr12w, Rr12d, Rr12},
        [Rr13d] = {Rnone, Rr13b, Rr13w, Rr13d, Rr13},
        [Rr14d] = {Rnone, Rr14b, Rr14w, Rr14d, Rr14},
        [Rr15d] = {Rnone, Rr15b, Rr15w, Rr15d, Rr15},

        [Rrax] = {Rnone, Ral,  Rax,  Reax, Rrax},
        [Rrcx] = {Rnone, Rcl,  Rcx,  Recx, Rrcx},
        [Rrdx] = {Rnone, Rdl,  Rdx,  Redx, Rrdx},
        [Rrbx] = {Rnone, Rbl,  Rbx,  Rebx, Rrbx},
        [Rrsi] = {Rnone, Rsil, Rsi,  Resi, Rrsi},
        [Rrdi] = {Rnone, Rsil, Rdi,  Redi, Rrdi},
        [Rr8]   = {Rnone, Rr8b,  Rr8w,  Rr8d,  Rr8},
        [Rr9]   = {Rnone, Rr9b,  Rr9w,  Rr9d,  Rr9},
        [Rr10]  = {Rnone, Rr10b, Rr10w, Rr10d, Rr10},
        [Rr11]  = {Rnone, Rr11b, Rr11w, Rr11d, Rr11},
        [Rr12]  = {Rnone, Rr12b, Rr12w, Rr12d, Rr12},
        [Rr13]  = {Rnone, Rr13b, Rr13w, Rr13d, Rr13},
        [Rr14]  = {Rnone, Rr14b, Rr14w, Rr14d, Rr14},
        [Rr15]  = {Rnone, Rr15b, Rr15w, Rr15d, Rr15},

        [Rxmm0f] = {[ModeF] = Rxmm0f, [ModeD] = Rxmm0d},
        [Rxmm1f] = {[ModeF] = Rxmm1f, [ModeD] = Rxmm1d},
        [Rxmm2f] = {[ModeF] = Rxmm2f, [ModeD] = Rxmm2d},
        [Rxmm3f] = {[ModeF] = Rxmm3f, [ModeD] = Rxmm3d},
        [Rxmm4f] = {[ModeF] = Rxmm4f, [ModeD] = Rxmm4d},
        [Rxmm5f] = {[ModeF] = Rxmm5f, [ModeD] = Rxmm5d},
        [Rxmm6f] = {[ModeF] = Rxmm6f, [ModeD] = Rxmm6d},
        [Rxmm7f] = {[ModeF] = Rxmm7f, [ModeD] = Rxmm7d},
        [Rxmm8f] = {[ModeF] = Rxmm8f, [ModeD] = Rxmm8d},
        [Rxmm9f] = {[ModeF] = Rxmm9f, [ModeD] = Rxmm9d},
        [Rxmm10f] = {[ModeF] = Rxmm0f, [ModeD] = Rxmm0d},
        [Rxmm11f] = {[ModeF] = Rxmm1f, [ModeD] = Rxmm1d},
        [Rxmm12f] = {[ModeF] = Rxmm2f, [ModeD] = Rxmm2d},
        [Rxmm13f] = {[ModeF] = Rxmm3f, [ModeD] = Rxmm3d},
        [Rxmm14f] = {[ModeF] = Rxmm4f, [ModeD] = Rxmm4d},
        [Rxmm15f] = {[ModeF] = Rxmm5f, [ModeD] = Rxmm5d},

        [Rxmm0d] = {[ModeF] = Rxmm0f, [ModeD] = Rxmm0d},
        [Rxmm1d] = {[ModeF] = Rxmm1f, [ModeD] = Rxmm1d},
        [Rxmm2d] = {[ModeF] = Rxmm2f, [ModeD] = Rxmm2d},
        [Rxmm3d] = {[ModeF] = Rxmm3f, [ModeD] = Rxmm3d},
        [Rxmm4d] = {[ModeF] = Rxmm4f, [ModeD] = Rxmm4d},
        [Rxmm5d] = {[ModeF] = Rxmm5f, [ModeD] = Rxmm5d},
        [Rxmm6d] = {[ModeF] = Rxmm6f, [ModeD] = Rxmm6d},
        [Rxmm7d] = {[ModeF] = Rxmm7f, [ModeD] = Rxmm7d},
        [Rxmm8d] = {[ModeF] = Rxmm8f, [ModeD] = Rxmm8d},
        [Rxmm9d] = {[ModeF] = Rxmm9f, [ModeD] = Rxmm9d},
        [Rxmm10d] = {[ModeF] = Rxmm0f, [ModeD] = Rxmm0d},
        [Rxmm11d] = {[ModeF] = Rxmm1f, [ModeD] = Rxmm1d},
        [Rxmm12d] = {[ModeF] = Rxmm2f, [ModeD] = Rxmm2d},
        [Rxmm13d] = {[ModeF] = Rxmm3f, [ModeD] = Rxmm3d},
        [Rxmm14d] = {[ModeF] = Rxmm4f, [ModeD] = Rxmm4d},
        [Rxmm15d] = {[ModeF] = Rxmm5f, [ModeD] = Rxmm5d},
    };

    assert(crtab[r][m] != Rnone);
    return locphysreg(crtab[r][m]);
}
