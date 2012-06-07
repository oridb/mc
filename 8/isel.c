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

/* instruction selection state */
typedef struct Isel Isel;
struct Isel {
    Insn **il;
    size_t ni;
    Node *ret;
    Htab *locs; /* decl id => int stkoff */
    Htab *globls; /* decl id => char *globlname */

    /* 6 general purpose regs */
    int rtaken[Nreg];
};

/* string tables */
const char *regnames[] = {
#define Reg(r, name, mode) name,
#include "regs.def"
#undef Reg
};

const Mode regmodes[] = {
#define Reg(r, name, mode) mode,
#include "regs.def"
#undef Reg
};

const Reg reginterferes[][Nmode + 1] = {
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
};

char *insnfmts[] = {
#define Insn(val, fmt, attr) fmt,
#include "insns.def"
#undef Insn
};

void locprint(FILE *fd, Loc *l);
void iprintf(FILE *fd, Insn *insn);

/* used to decide which operator is appropriate
 * for implementing various conditional operators */
struct {
    AsmOp test;
    AsmOp jmp;
    AsmOp getflag;
} reloptab[Numops] = {
    [Olnot] = {Itest, Ijz, Isetz},
    [Oeq] = {Icmp, Ijz, Isetz},
    [One] = {Icmp, Ijnz, Isetnz},
    [Ogt] = {Icmp, Ijg, Isetgt},
    [Oge] = {Icmp, Ijge, Isetge},
    [Olt] = {Icmp, Ijl, Isetlt},
    [Ole] = {Icmp, Ijle, Isetle}
};


/* forward decls */
Loc selexpr(Isel *s, Node *n);

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

Loc loc(Isel *s, Node *n)
{
    Loc l;
    Node *v;
    size_t stkoff;

    switch (exprop(n)) {
        case Ovar:
            if (hthas(s->locs, (void*)n->expr.did)) {
                stkoff = (size_t)htget(s->locs, (void*)n->expr.did);
                locmem(&l, -stkoff, Rebp, Rnone, ModeL);
            } else if (hthas(s->globls, (void*)n->expr.did)) {
                locstrlbl(&l, htget(s->globls, (void*)n->expr.did));
            } else {
                die("%s (id=%ld) not found", namestr(n->expr.args[0]), n->expr.did);
            }
            break;
        case Olit:
            v = n->expr.args[0];
            switch (v->lit.littype) {
                case Lchr:      loclit(&l, v->lit.chrval); break;
                case Lbool:     loclit(&l, v->lit.boolval); break;
                case Lint:      loclit(&l, v->lit.intval); break;
                default:
                                die("Literal type %s should be blob", litstr(v->lit.littype));
            }
            break;
        default:
            die("Node %s not leaf in loc()", opstr(exprop(n)));
            break;
    }
    return l;
}

Mode mode(Node *n)
{
    return ModeL;
}

Loc coreg(Loc r, Mode m)
{
    Loc l;

    Reg crtab[][Nmode + 1] = {
        [Ral] = {Rnone, Ral, Rax, Reax},
        [Rcl] = {Rnone, Rcl, Rcx, Recx},
        [Rdl] = {Rnone, Rdl, Rdx, Redx},
        [Rbl] = {Rnone, Rbl, Rbx, Rebx},

        [Rax] = {Rnone, Ral, Rax, Reax},
        [Rcx] = {Rnone, Rcl, Rcx, Recx},
        [Rdx] = {Rnone, Rdl, Rdx, Redx},
        [Rbx] = {Rnone, Rbl, Rbx, Rebx},
        [Rsi] = {Rnone, Rnone, Rsi, Resi},
        [Rdi] = {Rnone, Rnone, Rdi, Redi},

        [Reax] = {Rnone, Ral, Rax, Reax},
        [Recx] = {Rnone, Rcl, Rcx, Recx},
        [Redx] = {Rnone, Rdl, Rdx, Redx},
        [Rebx] = {Rnone, Rbl, Rbx, Rebx},
        [Resi] = {Rnone, Rnone, Rsi, Resi},
        [Redi] = {Rnone, Rnone, Rdi, Redi},
    };
    if (r.type != Locreg)
        die("Non-reg passed to coreg()");
    locreg(&l, crtab[r.reg][m]);
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

Insn *mkinsnv(AsmOp op, va_list ap)
{
    Loc *l;
    Insn *i;
    int n;

    n = 0;
    i = malloc(sizeof(Insn));
    i->op = op;
    if (op == Isetnz)
        breakhere();
    while ((l = va_arg(ap, Loc*)) != NULL)
        i->args[n++] = *l;
    i->narg = n;
    return i;
}

Insn *mkinsn(AsmOp op, ...)
{
    va_list ap;
    Insn *i;

    va_start(ap, op);
    i = mkinsnv(op, ap);
    va_end(ap);
    return i;
}

void g(Isel *s, AsmOp op, ...)
{
    va_list ap;
    Insn *i;

    va_start(ap, op);
    i = mkinsnv(op, ap);
    va_end(ap);
    lappend(&s->il, &s->ni, i);
}

void load(Isel *s, Loc *a, Loc *b)
{
    Loc l;
    assert(b->type == Locreg);
    if (a->type == Locreg)
        locmem(&l, 0, b->reg, Rnone, a->mode);
    else
        l = *a;
    g(s, Imov, &l, b, NULL);
}

void stor(Isel *s, Loc *a, Loc *b)
{
    Loc l;

    assert(a->type == Locreg || a->type == Loclit);
    if (b->type == Locreg)
        locmem(&l, 0, b->reg, Rnone, b->mode);
    else
        l = *b;
    g(s, Imov, a, &l, NULL);
}

/* ensures that a location is within a reg */
Loc inr(Isel *s, Loc a)
{
    Loc r;

    if (a.type == Locreg)
        return a;
    r = getreg(s, a.mode);
    load(s, &a, &r);
    return r;
}

/* ensures that a location is within a reg or an imm */
Loc inri(Isel *s, Loc a)
{
    if (a.type == Locreg || a.type == Loclit)
        return a;
    else
        return inr(s, a);
}

/* ensures that a location is within a reg or an imm */
Loc inrm(Isel *s, Loc a)
{
    if (a.type == Locreg || a.type == Locmem)
        return a;
    else
        return inr(s, a);
}

/* If we're testing equality, etc, it's a bit silly
 * to generate the test, store it to a bite, expand it
 * to the right width, and then test it again. Try to optimize
 * for these common cases.
 *
 * if we're doing the optimization to avoid
 * multiple tests, we want to eval the children
 * of the first arg, instead of the first arg
 * directly */
void selcjmp(Isel *s, Node *n, Node **args)
{
    Loc a, b;
    Loc l1, l2;
    AsmOp cond, jmp;

    cond = reloptab[exprop(args[0])].test;
    jmp = reloptab[exprop(args[0])].jmp;
    /* if we have a cond, we're knocking off the redundant test,
     * and want to eval the children */
    if (cond) {
        a = selexpr(s, args[0]->expr.args[1]);
        b = selexpr(s, args[0]->expr.args[0]);
        b = inr(s, b);
    } else {
        cond = Itest;
        jmp = Ijnz;
        a = inr(s, selexpr(s, args[0])); /* cond */
        b = a;
    }

    /* the jump targets will always be evaluated the same way */
    loclbl(&l1, args[1]); /* if true */
    loclbl(&l2, args[2]); /* if false */

    g(s, cond, &a, &b, NULL);
    g(s, jmp, &l1, NULL);
    g(s, Ijmp, &l2, NULL);
}

static Loc binop(Isel *s, AsmOp op, Node *x, Node *y)
{
    Loc a, b;

    a = selexpr(s, x);
    b = selexpr(s, y);
    a = inr(s, a);
    g(s, op, &b, &a, NULL);
    freeloc(s, b);
    return a;
}

static int ismergablemul(Node *n, int *r)
{
    int v;

    if (exprop(n) != Omul)
        return 0;
    if (exprop(n->expr.args[1]) != Olit)
        return 0;
    if (n->expr.args[1]->expr.args[0]->type != Nlit)
        return 0;
    if (n->expr.args[1]->expr.args[0]->lit.littype != Lint)
        return 0;
    v = n->expr.args[1]->expr.args[0]->lit.intval;
    if (v != 2 && v != 4 && v != 8)
        return 0;
    *r = v;
    return 1;
}
/* We have a few common cases to optimize here:
 *    Oadd(
 *        reg,
 *        reg||const))
 * or:
 *    Oadd(
 *        reg,
 *        Omul(reg,
 *             2 || 4 || 8)))
 */
static Loc memloc(Isel *s, Node *e, Mode m)
{
    Node **args;
    Loc l, b, o; /* location, base, offset */
    int scale;

    scale = 0;
    if (exprop(e) == Oadd) {
        args = e->expr.args;
        b = selexpr(s, args[0]);
        if (ismergablemul(args[1], &scale))
            o = selexpr(s, args[1]->expr.args[0]);
        else
            o = selexpr(s, args[1]);
        if (b.type != Locreg)
            b = inr(s, b);
        if (o.type == Loclit) {
            locmem(&l, o.lit, b.reg, Rnone, m);
        } else if (o.type == Locreg) {
            b = inr(s, b);
            locmems(&l, 0, b.reg, o.reg, scale, m);
        }
    } else {
        l = selexpr(s, e);
        if (l.type == Locreg)
            locmem(&l, 0, l.reg, Rnone, m);
    }
    return l;
}

Loc gencall(Isel *s, Node *n)
{
    int argsz, argoff;
    int i;
    Loc eax, esp;       /* hard-coded registers */
    Loc stkbump;        /* calculated stack offset */
    Loc dst, arg, fn;   /* values we reduced */

    locreg(&esp, Resp);
    locreg(&eax, Reax);
    claimreg(s, Reax);
    argsz = 0;
    /* Have to calculate the amount to bump the stack
     * pointer by in one pass first, otherwise if we push
     * one at a time, we evaluate the args in reverse order.
     * Not good.
     *
     * We skip the first operand, since it's the function itself */
    for (i = 1; i < n->expr.nargs; i++)
        argsz += size(n->expr.args[i]);
    loclit(&stkbump, argsz);
    if (argsz)
        g(s, Isub, &stkbump, &esp, NULL);

    /* Now, we can evaluate the arguments */
    argoff = 0;
    for (i = 1; i < n->expr.nargs; i++) {
        arg = selexpr(s, n->expr.args[i]);
        arg = inri(s, arg);
        locmem(&dst, argoff, Resp, Rnone, arg.mode);
        stor(s, &arg, &dst);
        argsz += size(n->expr.args[i]);
    }
    fn = selexpr(s, n->expr.args[0]);
    g(s, Icall, &fn, NULL);
    if (argsz)
        g(s, Iadd, &stkbump, &esp, NULL);
    return eax;
}

void blit(Isel *s, Loc a, Loc b, int sz)
{
    int i;
    Reg sp, dp; /* pointers to src, dst */
    Loc tmp, src, dst; /* source memory, dst memory */

    sp = inr(s, a).reg;
    dp = inr(s, b).reg;

    /* Slightly funny loop condition: We might have trailing bytes
     * that we can't blit word-wise. */
    tmp = getreg(s, ModeL);
    for (i = 0; i + 4 <= sz; i+= 4) {
        locmem(&src, i, sp, Rnone, ModeL);
        locmem(&dst, i, dp, Rnone, ModeL);
        g(s, Imov, &src, &tmp, NULL);
        g(s, Imov, &tmp, &dst, NULL);
    }
    /* now, the trailing bytes */
    tmp = coreg(tmp, ModeB);
    for (; i < sz; i++) {
        locmem(&src, i, sp, Rnone, ModeB);
        locmem(&dst, i, dp, Rnone, ModeB);
        g(s, Imov, &src, &tmp, NULL);
        g(s, Imov, &tmp, &dst, NULL);
    }
}

Loc selexpr(Isel *s, Node *n)
{
    Loc a, b, c, r;
    Loc eax, edx, cl; /* x86 wanst some hard-coded regs */
    Node **args;

    args = n->expr.args;
    r = (Loc){Locnone, };
    locreg(&eax, Reax);
    locreg(&edx, Redx);
    locreg(&cl, Rcl);
    switch (exprop(n)) {
        case Oadd:      r = binop(s, Iadd, args[0], args[1]); break;
        case Osub:      r = binop(s, Isub, args[0], args[1]); break;

        case Omul:
            /* these get clobbered by the mul insn */
            claimreg(s, Reax);
            claimreg(s, Redx);
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inr(s, b);
            c = coreg(eax, mode(n));
            g(s, Imov, &a, &c, NULL);
            g(s, Imul, &b, NULL);
            freereg(s, Redx);
            r = eax;
            break;
        case Odiv:
        case Omod:
            /* these get clobbered by the div insn */
            claimreg(s, Reax);
            claimreg(s, Redx);
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inr(s, b);
            c = coreg(eax, mode(n));
            g(s, Imov, &a, &c, NULL);
            g(s, Ixor, &edx, &edx, NULL);
            g(s, Idiv, &b, NULL);
            freereg(s, Redx);
            if (exprop(n) == Odiv)
                r = eax;
            else
                r = edx;
            break;
        case Oneg:
            r = selexpr(s, args[0]);
            r = inr(s, r);
            g(s, Ineg, &r, NULL);
            break;

        case Obor:      r = binop(s, Ior,  args[0], args[1]); break;
        case Oband:     r = binop(s, Iand, args[0], args[1]); break;
        case Obxor:     r = binop(s, Ixor, args[0], args[1]); break;
        case Obsl:      
        case Obsr:
            claimreg(s, Rcl); /* shift requires cl as it's arg. stupid. */
            a = selexpr(s, args[0]);
            a = inr(s, a);
            b = selexpr(s, args[1]);
            c = coreg(cl, b.mode);
            g(s, Imov, &b, &c, NULL);
            if (exprop(n) == Obsr) {
                if (istysigned(n->expr.type))
                    g(s, Isar, &cl, &a, NULL);
                else
                    g(s, Ishr, &cl, &a, NULL);
            } else {
                g(s, Ishl, &cl, &a, NULL);
            }
            freeloc(s, cl);
            freeloc(s, b);
            r = a;
            break;
        case Obnot:
            r = selexpr(s, args[0]);
            r = inrm(s, r);
            g(s, Inot, &r, NULL);
            break;

        case Oderef:
            a = selexpr(s, args[0]);
            a = inr(s, a);
            r = getreg(s, a.mode);
            locmem(&c, 0, a.reg, Rnone, a.mode);
            g(s, Imov, &c, &r, NULL);
            break;

        case Oaddr:
            a = selexpr(s, args[0]);
            r = getreg(s, ModeL);
            g(s, Ilea, &a, &r, NULL);
            break;

        case Olnot:
            a = selexpr(s, args[0]);
            b = getreg(s, ModeB);
            r = coreg(b, mode(n));
            g(s, reloptab[exprop(n)].test, &a, &a, NULL);
            g(s, reloptab[exprop(n)].getflag, &b, NULL);
            g(s, Imovz, &b, &r, NULL);
            break;

        case Oeq: case One: case Ogt: case Oge: case Olt: case Ole:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inr(s, b);
            c = getreg(s, ModeB);
            r = coreg(c, mode(n));
            g(s, reloptab[exprop(n)].test, &a, &b, NULL);
            g(s, reloptab[exprop(n)].getflag, &c, NULL);
            g(s, Imovz, &c, &r, NULL);
            return r;

        case Oasn:  /* relabel */
            die("Unimplemented op %s", opstr(exprop(n)));
            break;
        case Ostor: /* reg -> mem */
            a = memloc(s, args[0], mode(n));
            b = selexpr(s, args[1]);
            b = inri(s, b);
            g(s, Imov, &b, &a, NULL);
            r = b;
            break;
        case Oload: /* mem -> reg */
            b = memloc(s, args[0], mode(n));
            r = getreg(s, mode(n));
            g(s, Imov, &b, &r, NULL);
            break;

        case Ocall:
            r = gencall(s, n);
            break;
        case Ocast: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Ojmp:
            g(s, Ijmp, loclbl(&a, args[0]), NULL);
            break;
        case Ocjmp:
            selcjmp(s, n, args);
            break;

        case Olit: /* fall through */
        case Ovar:
            r = loc(s, n);
            break;
        case Olbl:
            loclbl(&r, args[0]);
            break;
        case Oblit:
            b = selexpr(s, args[0]);
            a = selexpr(s, args[1]);
            blit(s, a, b, args[2]->expr.args[0]->lit.intval);
            break;

        /* These operators should never show up in the reduced trees,
         * since they should have been replaced with more primitive
         * expressions by now */
        case Obad: case Oret: case Opreinc: case Opostinc: case Opredec:
        case Opostdec: case Olor: case Oland: case Oaddeq:
        case Osubeq: case Omuleq: case Odiveq: case Omodeq: case Oboreq:
        case Obandeq: case Obxoreq: case Obsleq: case Obsreq: case Omemb:
        case Oslice: case Oidx: case Osize: case Numops:
        case Oslbase: case Osllen:
            dump(n, stdout);
            die("Should not see %s in isel", opstr(exprop(n)));
            break;
    }
    return r;
}

void locprint(FILE *fd, Loc *l)
{

    switch (l->type) {
        case Loclbl:
            fprintf(fd, "%s", l->lbl);
            break;
        case Locreg:
            fprintf(fd, "%s", regnames[l->reg]);
            break;
        case Locmem:
        case Locmeml:
            if (l->type == Locmem) {
                if (l->mem.constdisp)
                    fprintf(fd, "%ld", l->mem.constdisp);
            } else {
                if (l->mem.lbldisp)
                    fprintf(fd, "%s", l->mem.lbldisp);
            }
            if (l->mem.base)
                fprintf(fd, "(%s", regnames[l->mem.base]);
            if (l->mem.idx)
                fprintf(fd, ",%s", regnames[l->mem.idx]);
            if (l->mem.scale)
                fprintf(fd, ",%d", l->mem.scale);
            if (l->mem.base)
                fprintf(fd, ")");
            break;
        case Loclit:
            fprintf(fd, "$%ld", l->lit);
            break;
        case Locnone:
            die("Bad location in locprint()");
            break;
    }
}

void modeprint(FILE *fd, Loc *l)
{
    char mode[] = {
        [ModeB] = 'b',
        [ModeS] = 's',
        [ModeL] = 'l',
        [ModeQ] = 'q'
    };
    fputc(mode[l->mode], fd);
}

void iprintf(FILE *fd, Insn *insn)
{
    char *p;
    int i;
    int modeidx;
    
    p = insnfmts[insn->op];
    i = 0;
    for (; *p; p++) {
        if (*p !=  '%') {
            fputc(*p, fd);
            continue;
        }

        /* %-formating */
        p++;
        switch (*p) {
            case '\0':
                goto done;
            case 'r':
            case 'm':
            case 'l':
            case 'x':
            case 'v':
                locprint(fd, &insn->args[i]);
                i++;
                break;
            case 't':
                modeidx = 0;
            default:
                if (isdigit(*p))
                    modeidx = strtol(p, &p, 10);

                if (*p == 't')
                    modeprint(fd, &insn->args[modeidx]);
                else
                    die("Invalid %%-specifier '%c'", *p);
                break;
        }
    }
done:
    return;
}

void isel(Isel *s, Node *n)
{
    Loc lbl;

    switch (n->type) {
        case Nlbl:
            g(s, Ilbl, loclbl(&lbl, n), NULL);
            break;
        case Nexpr:
            selexpr(s, n);
            break;
        case Ndecl:
            break;
        default:
            die("Bad node type in isel()");
            break;
    }
}

void prologue(Isel *s, size_t sz)
{
    Loc esp;
    Loc ebp;
    Loc stksz;

    locreg(&esp, Resp);
    locreg(&ebp, Rebp);
    loclit(&stksz, sz);
    g(s, Ipush, &ebp, NULL);
    g(s, Imov, &esp, &ebp, NULL);
    g(s, Isub, &stksz, &esp, NULL);
}

void epilogue(Isel *s)
{
    Loc esp, ebp, eax;
    Loc ret;

    locreg(&esp, Resp);
    locreg(&ebp, Rebp);
    locreg(&eax, Reax);
    if (s->ret) {
        ret = loc(s, s->ret);
        g(s, Imov, &ret, &eax, NULL);
    }
    g(s, Imov, &ebp, &esp, NULL);
    g(s, Ipop, &ebp, NULL);
    g(s, Iret, NULL);
}

static void writeasm(Func *fn, Isel *is, FILE *fd)
{
    int i;

    if (fn->isglobl)
        fprintf(fd, ".globl %s\n", fn->name);
    fprintf(fd, "%s:\n", fn->name);
    for (i = 0; i < is->ni; i++)
        iprintf(fd, is->il[i]);
}

/* genasm requires all nodes in 'nl' to map cleanly to operations that are
 * natively supported, as promised in the output of reduce().  No 64-bit
 * operations on x32, no structures, and so on. */
void genasm(FILE *fd, Func *fn, Htab *globls)
{
    struct Isel is = {0,};
    int i;

    is.locs = fn->locs;
    is.globls = globls;
    is.ret = fn->ret;

    prologue(&is, fn->stksz);
    for (i = 0; i < fn->nn; i++) {
        bzero(is.rtaken, sizeof is.rtaken);
        isel(&is, fn->nl[i]);
    }
    epilogue(&is);

    if (debug)
      writeasm(fn, &is, stdout);

    writeasm(fn, &is, fd);
}
