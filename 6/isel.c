#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
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
#include "../config.h"

/* string tables */
char *insnfmts[] = {
#define Insn(val, fmt, use, def) fmt,
#include "insns.def"
#undef Insn
};

char* modenames[] = {
  [ModeB] = "b",
  [ModeW] = "w",
  [ModeL] = "l",
  [ModeQ] = "q",
  [ModeF] = "s",
  [ModeD] = "d"
};

/* forward decls */
Loc *selexpr(Isel *s, Node *n);
static size_t writeblob(FILE *fd, Htab *globls, Htab *strtab, Node *blob);

/* used to decide which operator is appropriate
 * for implementing various conditional operators */
struct {
    AsmOp test;
    AsmOp jmp;
    AsmOp getflag;
} reloptab[Numops] = {
    [Olnot] = {Itest, Ijz, Isetz}, /* lnot invalid for floats */
    /* signed int */
    [Oeq] = {Icmp, Ijz,  Isetz},
    [One] = {Icmp, Ijnz, Isetnz},
    [Ogt] = {Icmp, Ijg,  Isetg},
    [Oge] = {Icmp, Ijge, Isetge},
    [Olt] = {Icmp, Ijl,  Isetl},
    [Ole] = {Icmp, Ijle, Isetle},
    /* unsigned int */
    [Oueq] = {Icmp, Ijz,  Isetz},
    [Oune] = {Icmp, Ijnz, Isetnz},
    [Ougt] = {Icmp, Ija,  Iseta},
    [Ouge] = {Icmp, Ijae, Isetae},
    [Oult] = {Icmp, Ijb,  Isetb},
    [Oule] = {Icmp, Ijbe, Isetbe},
    /* float */
    [Ofeq] = {Icomis, Ijz,  Isetz},
    [Ofne] = {Icomis, Ijnz, Isetnz},
    [Ofgt] = {Icomis, Ija,  Iseta},
    [Ofge] = {Icomis, Ijae, Isetae},
    [Oflt] = {Icomis, Ijb,  Isetb},
    [Ofle] = {Icomis, Ijbe, Isetbe},
};

static Mode mode(Node *n)
{
    Type *t;

    t = tybase(exprtype(n));
    /* FIXME: What should the mode for, say, structs be when we have no
     * intention of loading /through/ the pointer? For now, we'll just say it's
     * the pointer mode, since we expect to address through the pointer */
    switch (t->type) {
        case Tyfloat32: return ModeF; break;
        case Tyfloat64: return ModeD; break;
        default:
            if (stacktype(t))
                return ModeQ;
            switch (size(n)) {
                case 1: return ModeB; break;
                case 2: return ModeW; break;
                case 4: return ModeL; break;
                case 8: return ModeQ; break;
            }
            break;
    }
    return ModeQ;
}

static int isintmode(Mode m)
{
    return m == ModeB || m == ModeW || m == ModeL || m == ModeQ;
}

static int isfloatmode(Mode m)
{
    return m == ModeF || m == ModeD;
}

static Loc *loc(Isel *s, Node *n)
{
    ssize_t stkoff;
    Loc *l, *rip;
    Node *v;

    switch (exprop(n)) {
        case Ovar:
            if (hthas(s->globls, n)) {
                rip = locphysreg(Rrip);
                l = locmeml(htget(s->globls, n), rip, NULL, mode(n));
            } else if (hthas(s->stkoff, n)) {
                stkoff = ptoi(htget(s->stkoff, n));
                l = locmem(-stkoff, locphysreg(Rrbp), NULL, mode(n));
            }  else {
                if (!hthas(s->reglocs, n))
                    htput(s->reglocs, n, locreg(mode(n)));
                return htget(s->reglocs, n);
            }
            break;
        case Olit:
            v = n->expr.args[0];
            switch (v->lit.littype) {
                case Lchr:      l = loclit(v->lit.chrval, mode(n)); break;
                case Lbool:     l = loclit(v->lit.boolval, mode(n)); break;
                case Lint:      l = loclit(v->lit.intval, mode(n)); break;
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

static Insn *mkinsnv(AsmOp op, va_list ap)
{
    Loc *l;
    Insn *i;
    int n;
    static size_t insnid;

    n = 0;
    i = malloc(sizeof(Insn));
    i->op = op;
    i->uid = insnid++;
    while ((l = va_arg(ap, Loc*)) != NULL)
        i->args[n++] = l;
    i->nargs = n;
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
    if (debugopt['i']) {
        printf("GEN ");
        iprintf(stdout, i);
    }
    lappend(&s->curbb->il, &s->curbb->ni, i);
}

static void movz(Isel *s, Loc *src, Loc *dst)
{
    if (src->mode == dst->mode)
        g(s, Imov, src, dst, NULL);
    else
        g(s, Imovzx, src, dst, NULL);
}

static void load(Isel *s, Loc *a, Loc *b)
{
    Loc *l;

    assert(b->type == Locreg);
    if (a->type == Locreg)
        l = locmem(0, b, Rnone, a->mode);
    else
        l = a;
    if (isfloatmode(b->mode))
        g(s, Imovs, l, b, NULL);
    else
        g(s, Imov, l, b, NULL);
}

static void stor(Isel *s, Loc *a, Loc *b)
{
    Loc *l;

    assert(a->type == Locreg || a->type == Loclit);
    if (b->type == Locreg)
        l = locmem(0, b, Rnone, b->mode);
    else
        l = b;
    if (isfloatmode(b->mode))
        g(s, Imovs, a, l, NULL);
    else
        g(s, Imov, a, l, NULL);
}

/* ensures that a location is within a reg */
static Loc *newr(Isel *s, Loc *a)
{
    Loc *r;

    r = locreg(a->mode);
    if (a->type == Locreg) {
        if (isfloatmode(a->mode))
            g(s, Imovs, a, r, NULL);
        else
            g(s, Imov, a, r, NULL);
    } else {
        load(s, a, r);
    }
    return r;
}

static Loc *inr(Isel *s, Loc *a)
{
    if (a->type == Locreg)
        return a;
    return newr(s, a);
}

/* ensures that a location is within a reg or an imm */
static Loc *inri(Isel *s, Loc *a)
{
    if (a->type == Locreg || a->type == Loclit)
        return a;
    else
        return newr(s, a);
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
static void selcjmp(Isel *s, Node *n, Node **args)
{
    Loc *a, *b;
    Loc *l1, *l2;
    AsmOp cond, jmp;

    cond = reloptab[exprop(args[0])].test;
    jmp = reloptab[exprop(args[0])].jmp;
    /* if we have a cond, we're knocking off the redundant test,
     * and want to eval the children */
    if (cond) {
        a = selexpr(s, args[0]->expr.args[0]);
        if (args[0]->expr.nargs == 2)
            b = selexpr(s, args[0]->expr.args[1]);
        else
            b = a;
        a = newr(s, a);
    } else {
        cond = Itest;
        jmp = Ijnz;
        b = newr(s, selexpr(s, args[0])); /* cond */
        a = b;
    }

    /* the jump targets will always be evaluated the same way */
    l1 = loclbl(args[1]); /* if true */
    l2 = loclbl(args[2]); /* if false */

    g(s, cond, b, a, NULL);
    g(s, jmp, l1, NULL);
    g(s, Ijmp, l2, NULL);
}

static Loc *binop(Isel *s, AsmOp op, Node *x, Node *y)
{
    Loc *a, *b;

    a = selexpr(s, x);
    b = selexpr(s, y);
    a = newr(s, a);
    g(s, op, b, a, NULL);
    return a;
}

/* We have a few common cases to optimize here:
 *    Oaddr(expr)
 * or:
 *    Oadd(
 *        reg,
 *        reg||const)
 *
 * or:
 *    Oadd(
 *        reg,
 *        Omul(reg,
 *             2 || 4 || 8)))
 */
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

static Loc *memloc(Isel *s, Node *e, Mode m)
{
    Node **args;
    Loc *l, *b, *o; /* location, base, offset */
    int scale;

    scale = 1;
    l = NULL;
    args = e->expr.args;
    if (exprop(e) == Oadd) {
        b = selexpr(s, args[0]);
        if (ismergablemul(args[1], &scale))
            o = selexpr(s, args[1]->expr.args[0]);
        else
            o = selexpr(s, args[1]);

        if (b->type != Locreg)
            b = inr(s, b);
        if (o->type == Loclit) {
            l = locmem(scale*o->lit, b, Rnone, m);
        } else {
            b = inr(s, b);
            o = inr(s, o);
            l = locmems(0, b, o, scale, m);
        }
    } else {
        l = selexpr(s, e);
        l = inr(s, l);
        l = locmem(0, l, Rnone, m);
    }
    assert(l != NULL);
    return l;
}

static void blit(Isel *s, Loc *to, Loc *from, size_t dstoff, size_t srcoff, size_t sz)
{
    Loc *sp, *dp, *len; /* pointers to src, dst */

    len = loclit(sz, ModeQ);
    sp = newr(s, from);
    dp = newr(s, to);

    /* length to blit */
    g(s, Imov, len, locphysreg(Rrcx), NULL);
    /* source address with offset */
    if (srcoff)
        g(s, Ilea, locmem(srcoff, sp, NULL, ModeQ), locphysreg(Rrsi), NULL);
    else
        g(s, Imov, sp, locphysreg(Rrsi), NULL);
    /* dest address with offset */
    if (dstoff)
        g(s, Ilea, locmem(dstoff, dp, NULL, ModeQ), locphysreg(Rrdi), NULL);
    else
        g(s, Imov, dp, locphysreg(Rrdi), NULL);
    g(s, Irepmovsb, NULL);
}

static int isfunc(Isel *s, Node *n)
{
    Node *d;

    if (exprop(n) != Ovar)
        return 0;
    if (!hthas(s->globls, n))
        return 0;
    d = decls[n->expr.did];
    if (d && d->decl.isconst)
        return tybase(decltype(d))->type == Tyfunc;
    return 0;
}

static void call(Isel *s, Node *n)
{
    AsmOp op;
    Loc *f;

    if (isfunc(s, n)) {
        op = Icall;
        f = locmeml(htget(s->globls, n), NULL, NULL, mode(n));
    } else {
        op = Icallind;
        f = selexpr(s, n);
    }
    g(s, op, f, NULL);
}

static Loc *gencall(Isel *s, Node *n)
{
    Loc *src, *dst, *arg;  /* values we reduced */
    Loc *retloc, *rsp, *ret;       /* hard-coded registers */
    Loc *stkbump;        /* calculated stack offset */
    int argsz, argoff;
    size_t i;

    rsp = locphysreg(Rrsp);
    if (tybase(exprtype(n))->type == Tyvoid) {
        retloc = NULL;
        ret = NULL;
    } else if (stacktype(exprtype(n))) {
        retloc = locphysreg(Rrax);
        ret = locreg(ModeQ);
    } else if (floattype(exprtype(n))) {
        retloc = coreg(Rxmm0d, mode(n));
        ret = locreg(mode(n));
    } else {
        retloc = coreg(Rrax, mode(n));
        ret = locreg(mode(n));
    }
    argsz = 0;
    /* Have to calculate the amount to bump the stack
     * pointer by in one pass first, otherwise if we push
     * one at a time, we evaluate the args in reverse order.
     * Not good.
     *
     * Skip the first operand, since it's the function itself */
    for (i = 1; i < n->expr.nargs; i++) {
        argsz = align(argsz, min(size(n->expr.args[i]), Ptrsz));
        argsz += size(n->expr.args[i]);
    }
    argsz = align(argsz, 16);
    stkbump = loclit(argsz, ModeQ);
    if (argsz)
        g(s, Isub, stkbump, rsp, NULL);

    /* Now, we can evaluate the arguments */
    argoff = 0;
    for (i = 1; i < n->expr.nargs; i++) {
        arg = selexpr(s, n->expr.args[i]);
        argoff = align(argoff, min(size(n->expr.args[i]), Ptrsz));
        if (stacknode(n->expr.args[i])) {
            src = locreg(ModeQ);
            g(s, Ilea, arg, src, NULL);
            blit(s, rsp, src, argoff, 0, size(n->expr.args[i]));
        } else {
            dst = locmem(argoff, rsp, NULL, arg->mode);
            arg = inri(s, arg);
            stor(s, arg, dst);
        }
        argoff += size(n->expr.args[i]);
    }
    call(s, n->expr.args[0]);
    if (argsz)
        g(s, Iadd, stkbump, rsp, NULL);
    if (retloc) {
        if (isfloatmode(retloc->mode))
            g(s, Imovs, retloc, ret, NULL);
        else
            g(s, Imov, retloc, ret, NULL);
    }
    return ret;
}

Loc *selexpr(Isel *s, Node *n)
{
    Loc *a, *b, *c, *d, *r;
    Loc *eax, *edx, *cl; /* x86 wants some hard-coded regs */
    Node **args;

    args = n->expr.args;
    eax = locphysreg(Reax);
    edx = locphysreg(Redx);
    cl = locphysreg(Rcl);
    r = NULL;
    switch (exprop(n)) {
        case Oadd:      r = binop(s, Iadd, args[0], args[1]); break;
        case Osub:      r = binop(s, Isub, args[0], args[1]); break;
        case Obor:      r = binop(s, Ior,  args[0], args[1]); break;
        case Oband:     r = binop(s, Iand, args[0], args[1]); break;
        case Obxor:     r = binop(s, Ixor, args[0], args[1]); break;
        case Omul:
            if (size(args[0]) == 1) {
                a = selexpr(s, args[0]);
                b = selexpr(s, args[1]);

                c = locphysreg(Ral);
                r = locreg(a->mode);
                g(s, Imov, a, c, NULL);
                g(s, Iimul_r, b, NULL);
                g(s, Imov, c, r, NULL);
            } else {
                r = binop(s, Iimul, args[0], args[1]);
            }
            break;
        case Odiv:
        case Omod:
            /* these get clobbered by the div insn */
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = newr(s, b);
            c = coreg(Reax, mode(n));
            r = locreg(a->mode);
            if (r->mode == ModeB)
                g(s, Ixor, eax, eax, NULL);
            else
                g(s, Ixor, edx, edx, NULL);
            g(s, Imov, a, c, NULL);
            g(s, Idiv, b, NULL);
            if (exprop(n) == Odiv)
                d = coreg(Reax, mode(n));
            else if (r->mode != ModeB)
                d = coreg(Redx, mode(n));
            else
                d = locphysreg(Rah);
            g(s, Imov, d, r, NULL);
            break;
        case Oneg:
            r = selexpr(s, args[0]);
            r = newr(s, r);
            g(s, Ineg, r, NULL);
            break;

        /* fp expressions */
        case Ofadd:      r = binop(s, Iadds, args[0], args[1]); break;
        case Ofsub:      r = binop(s, Isubs, args[0], args[1]); break;
        case Ofmul:      r = binop(s, Imuls, args[0], args[1]); break;
        case Ofdiv:      r = binop(s, Idivs, args[0], args[1]); break;
        case Ofneg:
            r = selexpr(s, args[0]);
            r = newr(s, r);
            a = NULL;
            b = NULL;
            if (mode(args[0]) == ModeF) {
                a = locreg(ModeF);
                b = loclit(1LL << (31), ModeF);
                g(s, Imovs, r, a);
            } else if (mode(args[0]) == ModeD) {
                a = locreg(ModeQ);
                b = loclit(1LL << 63, ModeQ);
                g(s, Imov, r, a, NULL);
            }
            g(s, Ixor, b, a, NULL);
            g(s, Imov, a, r, NULL);
            break;
        case Obsl:
        case Obsr:
            a = newr(s, selexpr(s, args[0]));
            b = selexpr(s, args[1]);
            if (b->type == Loclit) {
                d = b;
            } else {
                c = coreg(Rcl, b->mode);
                g(s, Imov, b, c, NULL);
                d = cl;
            }
            if (exprop(n) == Obsr) {
                if (istysigned(n->expr.type))
                    g(s, Isar, d, a, NULL);
                else
                    g(s, Ishr, d, a, NULL);
            } else {
                g(s, Ishl, d, a, NULL);
            }
            r = a;
            break;
        case Obnot:
            r = selexpr(s, args[0]);
            r = newr(s, r);
            g(s, Inot, r, NULL);
            break;

        case Oderef:
            r = memloc(s, args[0], mode(n));
            break;

        case Oaddr:
            a = selexpr(s, args[0]);
            if (a->type == Loclbl || (a->type == Locmeml && !a->mem.base)) {
                r = loclitl(a->lbl);
            } else {
                r = locreg(ModeQ);
                g(s, Ilea, a, r, NULL);
            }
            break;

        case Olnot:
            a = newr(s, selexpr(s, args[0]));
            b = locreg(ModeB);
            r = locreg(mode(n));
            /* lnot only valid for integer-like values */
            g(s, reloptab[exprop(n)].test, a, a, NULL);
            g(s, reloptab[exprop(n)].getflag, b, NULL);
            movz(s, b, r);
            break;

        case Oeq: case One: case Ogt: case Oge: case Olt: case Ole:
        case Ofeq: case Ofne: case Ofgt: case Ofge: case Oflt: case Ofle:
        case Oueq: case Oune: case Ougt: case Ouge: case Oult: case Oule:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            a = newr(s, a);
            c = locreg(ModeB);
            r = locreg(mode(n));
            g(s, reloptab[exprop(n)].test, b, a, NULL);
            g(s, reloptab[exprop(n)].getflag, c, NULL);
            movz(s, c, r);
            return r;

        case Oasn:  /* relabel */
            die("Unimplemented op %s", opstr(exprop(n)));
            break;
        case Oset:
            assert(exprop(args[0]) == Ovar || exprop(args[0]) == Oderef);
            b = selexpr(s, args[1]);
            if (exprop(args[0]) == Oderef)
                a = memloc(s, args[0]->expr.args[0], mode(n));
            else
                a = selexpr(s, args[0]);
            b = inri(s, b);
            if (isfloatmode(b->mode))
                g(s, Imovs, b, a, NULL);
            else
                g(s, Imov, b, a, NULL);
            r = b;
            break;
        case Ocall:
            r = gencall(s, n);
            break;
        case Ojmp:
            g(s, Ijmp, a = loclbl(args[0]), NULL);
            break;
        case Ocjmp:
            selcjmp(s, n, args);
            break;

        case Olit: /* fall through */
            r = loc(s, n);
            break;
        case Ovar:
            if (isfunc(s, n)) {
                r = locreg(ModeQ);
                a = loc(s, n);
                g(s, Ilea, a, r, NULL);
            } else {
                r = loc(s, n);
            }
            break;
        case Olbl:
            r = loclbl(args[0]);
            break;
        case Oblit:
            a = selexpr(s, args[0]);
            r = selexpr(s, args[1]);
            blit(s, a, r, 0, 0, args[2]->expr.args[0]->lit.intval);
            break;
        case Otrunc:
            a = selexpr(s, args[0]);
            a = inr(s, a);
            r = locreg(mode(n));
            g(s, Imov, a, r, NULL);
            break;
        case Ozwiden:
            a = selexpr(s, args[0]);
            a = inr(s, a);
            r = locreg(mode(n));
            movz(s, a, r);
            break;
        case Oswiden:
            a = selexpr(s, args[0]);
            a = inr(s, a);
            r = locreg(mode(n));
            g(s, Imovsx, a, r, NULL);
            break;
        case Oint2flt:
            a = selexpr(s, args[0]);
            b = locreg(ModeQ);
            r = locreg(mode(n));
            g(s, Imovs, a, b, NULL);
            g(s, Icvttsi2sd, b, r, NULL);
            break;
        case Oflt2int:
            a = selexpr(s, args[0]);
            b = locreg(ModeQ);
            r = locreg(mode(n));
            g(s, Icvttsd2si, a, b, NULL);
            g(s, Imov, b, r, NULL);
            break;

        /* These operators should never show up in the reduced trees,
         * since they should have been replaced with more primitive
         * expressions by now */
        case Obad: case Oret: case Opreinc: case Opostinc: case Opredec:
        case Opostdec: case Olor: case Oland: case Oaddeq:
        case Osubeq: case Omuleq: case Odiveq: case Omodeq: case Oboreq:
        case Obandeq: case Obxoreq: case Obsleq: case Obsreq: case Omemb:
        case Oslice: case Oidx: case Osize: case Numops:
        case Oucon: case Ouget: case Otup: case Oarr: case Ostruct:
        case Oslbase: case Osllen: case Ocast:
        case Obreak: case Ocontinue:
            dump(n, stdout);
            die("Should not see %s in isel", opstr(exprop(n)));
            break;
    }
    return r;
}

void locprint(FILE *fd, Loc *l, char spec)
{
    assert(l->mode);
    switch (l->type) {
        case Loclitl:
            assert(spec == 'i' || spec == 'x' || spec == 'u');
            fprintf(fd, "$%s", l->lbl);
            break;
        case Loclbl:
            assert(spec == 'm' || spec == 'v' || spec == 'x');
            fprintf(fd, "%s", l->lbl);
            break;
        case Locreg:
            assert((spec == 'r' && isintmode(l->mode)) || 
                   (spec == 'f' && isfloatmode(l->mode)) ||
                   spec == 'v' ||
                   spec == 'x' ||
                   spec == 'u');
            if (l->reg.colour == Rnone)
                fprintf(fd, "%%P.%zd", l->reg.id);
            else
                fprintf(fd, "%s", regnames[l->reg.colour]);
            break;
        case Locmem:
        case Locmeml:
            assert(spec == 'm' || spec == 'v' || spec == 'x');
            if (l->type == Locmem) {
                if (l->mem.constdisp)
                    fprintf(fd, "%ld", l->mem.constdisp);
            } else {
                if (l->mem.lbldisp)
                    fprintf(fd, "%s", l->mem.lbldisp);
            }
            if (l->mem.base) {
                fprintf(fd, "(");
                locprint(fd, l->mem.base, 'r');
                if (l->mem.idx) {
                    fprintf(fd, ",");
                    locprint(fd, l->mem.idx, 'r');
                }
                if (l->mem.scale > 1)
                    fprintf(fd, ",%d", l->mem.scale);
                if (l->mem.base)
                    fprintf(fd, ")");
            } else if (l->type != Locmeml) {
                die("Only locmeml can have unspecified base reg");
            }
            break;
        case Loclit:
            assert(spec == 'i' || spec == 'x' || spec == 'u');
            fprintf(fd, "$%ld", l->lit);
            break;
        case Locnone:
            die("Bad location in locprint()");
            break;
    }
}

int subreg(Loc *a, Loc *b)
{
    return rclass(a) == rclass(b) && a->mode != b->mode;
}

void iprintf(FILE *fd, Insn *insn)
{
    char *p;
    int i;
    int modeidx;

    /* x64 has a quirk; it has no movzlq because mov zero extends. This
     * means that we need to do a movl when we really want a movzlq. Since
     * we don't know the name of the reg to use, we need to sub it in when
     * writing... */
    switch (insn->op) {
        case Imovzx:
            if (insn->args[0]->mode == ModeL && insn->args[1]->mode == ModeQ) {
                if (insn->args[1]->reg.colour) {
                    insn->op = Imov;
                    insn->args[1] = coreg(insn->args[1]->reg.colour, ModeL);
                }
            }
            break;
        case Imovs:
            /* moving a reg to itself is dumb. */
            if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
                return;
            break;
        case Imov:
            assert(!isfloatmode(insn->args[1]->mode));
            if (insn->args[0]->type != Locreg || insn->args[1]->type != Locreg)
                break;
            if (insn->args[0]->reg.colour == Rnone || insn->args[1]->reg.colour == Rnone)
                break;
            /* if one reg is a subreg of another, we can just use the right
             * mode to move between them. */
            if (subreg(insn->args[0], insn->args[1]))
                insn->args[0] = coreg(insn->args[0]->reg.colour, insn->args[1]->mode);
            /* moving a reg to itself is dumb. */
            if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
                return;
            break;
        default:
            break;
    }
    p = insnfmts[insn->op];
    i = 0;
    modeidx = 0;
    for (; *p; p++) {
        if (*p !=  '%') {
            fputc(*p, fd);
            continue;
        }

        /* %-formating */
        p++;
        switch (*p) {
            case '\0':
                goto done; /* skip the final p++ */
            case 'r': /* int register */
            case 'f': /* float register */
            case 'm': /* memory */
            case 'i': /* imm */
            case 'v': /* reg/mem */
            case 'u': /* reg/imm */
            case 'x': /* reg/mem/imm */
                locprint(fd, insn->args[i], *p);
                i++;
                break;
            case 't':
            default:
                /* the  asm description uses 1-based indexing, so that 0
                 * can be used as a sentinel. */
                if (isdigit(*p))
                    modeidx = strtol(p, &p, 10) - 1;

                if (*p == 't')
                    fputs(modenames[insn->args[modeidx]->mode], fd);
                else
                    die("Invalid %%-specifier '%c'", *p);
                break;
        }
    }
done:
    return;
}

static void isel(Isel *s, Node *n)
{
    switch (n->type) {
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

Reg savedregs[] = {
    Rrcx, Rrdx, Rrbx, Rrsi, Rrdi, Rr8, Rr9, Rr10, Rr11, Rr12, Rr13, Rr14, Rr15,
    /*
    Rxmm0d, Rxmm1d, Rxmm2d, Rxmm3d, Rxmm4d, Rxmm5d, Rxmm6d, Rxmm7d,
    Rxmm8d, Rxmm9d, Rxmm10d, Rxmm11d, Rxmm12d, Rxmm13d, Rxmm14d, Rxmm15d,
    */
};

static void prologue(Isel *s, size_t sz)
{
    Loc *rsp;
    Loc *rbp;
    Loc *stksz;
    size_t i;

    rsp = locphysreg(Rrsp);
    rbp = locphysreg(Rrbp);
    stksz = loclit(sz, ModeQ);
    /* enter function */
    g(s, Ipush, rbp, NULL);
    g(s, Imov, rsp, rbp, NULL);
    g(s, Isub, stksz, rsp, NULL);
    /* save registers */
    for (i = 0; i < sizeof(savedregs)/sizeof(savedregs[0]); i++) {
        s->calleesave[i] = locreg(ModeQ);
        g(s, Imov, locphysreg(savedregs[i]), s->calleesave[i], NULL);
    }
    s->stksz = stksz; /* need to update if we spill */
}

static void epilogue(Isel *s)
{
    Loc *rsp, *rbp;
    Loc *ret;
    size_t i;

    rsp = locphysreg(Rrsp);
    rbp = locphysreg(Rrbp);
    if (s->ret) {
        ret = loc(s, s->ret);
        if (floattype(exprtype(s->ret)))
            g(s, Imovs, ret, coreg(Rxmm0d, ret->mode), NULL);
        else
            g(s, Imov, ret, coreg(Rax, ret->mode), NULL);
    }
    /* restore registers */
    for (i = 0; i < Nsaved; i++)
        g(s, Imov, s->calleesave[i], locphysreg(savedregs[i]), NULL);
    /* leave function */
    g(s, Imov, rbp, rsp, NULL);
    g(s, Ipop, rbp, NULL);
    g(s, Iret, NULL);
}

static void writeasm(FILE *fd, Isel *s, Func *fn)
{
    size_t i, j;

    if (fn->isexport || !strcmp(fn->name, symprefix "main"))
        fprintf(fd, ".globl %s\n", fn->name);
    fprintf(fd, "%s:\n", fn->name);
    for (j = 0; j < s->cfg->nbb; j++) {
        for (i = 0; i < s->bb[j]->nlbls; i++)
            fprintf(fd, "%s:\n", s->bb[j]->lbls[i]);
        for (i = 0; i < s->bb[j]->ni; i++)
            iprintf(fd, s->bb[j]->il[i]);
    }
}

static Asmbb *mkasmbb(Bb *bb)
{
    Asmbb *as;

    as = zalloc(sizeof(Asmbb));
    as->id = bb->id;
    as->pred = bsdup(bb->pred);
    as->succ = bsdup(bb->succ);
    as->lbls = memdup(bb->lbls, bb->nlbls*sizeof(char*));
    as->nlbls = bb->nlbls;
    return as;
}

static void writebytes(FILE *fd, char *p, size_t sz)
{
    size_t i;

    for (i = 0; i < sz; i++) {
        if (i % 60 == 0)
            fprintf(fd, "\t.ascii \"");
        if (p[i] == '"' || p[i] == '\\')
            fprintf(fd, "\\");
        if (isprint(p[i]))
            fprintf(fd, "%c", p[i]);
        else
            fprintf(fd, "\\%03o", (uint8_t)p[i] & 0xff);
        /* line wrapping for readability */
        if (i % 60 == 59 || i == sz - 1)
            fprintf(fd, "\"\n");
    }
}

static size_t writelit(FILE *fd, Htab *strtab, Node *v, Type *ty)
{
    char buf[128];
    char *lbl;
    size_t sz;
    char *intsz[] = {
        [1] = ".byte",
        [2] = ".short",
        [4] = ".long",
        [8] = ".quad"
    };
    union {
        float fv;
        double dv;
        uint64_t qv;
        uint32_t lv;
    } u;

    assert(v->type == Nlit);
    sz = tysize(ty);
    switch (v->lit.littype) {
        case Lint:      fprintf(fd, "\t%s %lld\n", intsz[sz], v->lit.intval);    break;
        case Lbool:     fprintf(fd, "\t.byte %d\n", v->lit.boolval);     break;
        case Lchr:      fprintf(fd, "\t.long %d\n",  v->lit.chrval);     break;
        case Lflt:
                if (tybase(v->lit.type)->type == Tyfloat32) {
                    u.fv = v->lit.fltval;
                    fprintf(fd, "\t.long 0x%"PRIx32"\n", u.lv);
                } else if (tybase(v->lit.type)->type == Tyfloat64) {
                    u.dv = v->lit.fltval;
                    fprintf(fd, "\t.quad 0x%"PRIx64"\n", u.qv);
                }
                break;
        case Lstr:
           if (hthas(strtab, v->lit.strval)) {
               lbl = htget(strtab, v->lit.strval);
           } else {
               lbl = genlblstr(buf, sizeof buf);
               htput(strtab, v->lit.strval, strdup(lbl));
           }
           fprintf(fd, "\t.quad %s\n", lbl);
           fprintf(fd, "\t.quad %zd\n", strlen(v->lit.strval));
           break;
        case Lfunc:
            die("Generating this shit ain't ready yet ");
            break;
        case Llbl:
            die("Can't generate literal labels, ffs. They're not data.");
            break;
    }
    return sz;
}

static size_t writepad(FILE *fd, size_t sz)
{
    assert((ssize_t)sz >= 0);
    if (sz > 0)
        fprintf(fd, "\t.fill %zd,1,0\n", sz);
    return sz;
}

static size_t getintlit(Node *n, char *failmsg)
{
    if (exprop(n) != Olit)
        fatal(n->line, "%s", failmsg);
    n = n->expr.args[0];
    if (n->lit.littype != Lint)
        fatal(n->line, "%s", failmsg);
    return n->lit.intval;
}

static size_t writeslice(FILE *fd, Htab *globls, Htab *strtab, Node *n)
{
    Node *base, *lo, *hi;
    ssize_t loval, hival, sz;
    char *lbl;

    base = n->expr.args[0];
    lo = n->expr.args[1];
    hi = n->expr.args[2];

    /* by this point, all slicing operations should have had their bases
     * pulled out, and we should have vars with their pseudo-decls in their
     * place */
    if (exprop(base) != Ovar || !base->expr.isconst)
        fatal(base->line, "slice base is not a constant value");
    loval = getintlit(lo, "lower bound in slice is not constant literal");
    hival = getintlit(hi, "upper bound in slice is not constant literal");
    sz = tysize(tybase(exprtype(base))->sub[0]);

    lbl = htget(globls, base);
    fprintf(fd, "\t.quad %s + (%zd*%zd)\n", lbl, loval, sz);
    fprintf(fd, "\t.quad %zd\n", (hival - loval));
    return size(n);
}

static size_t writestruct(FILE *fd, Htab *globls, Htab *strtab, Node *n)
{
    Type *t;
    Node **dcl;
    int found;
    size_t i, j;
    size_t sz, pad, end;
    size_t ndcl;

    sz = 0;
    t = tybase(exprtype(n));
    assert(t->type == Tystruct);
    dcl = t->sdecls;
    ndcl = t->nmemb;
    for (i = 0; i < ndcl; i++) {
        pad = alignto(sz, decltype(dcl[i]));
        sz += writepad(fd, pad - sz);
        found = 0;
        for (j = 0; j < n->expr.nargs; j++)
            if (!strcmp(namestr(n->expr.args[j]->expr.idx), declname(dcl[i]))) {
                found = 1;
                sz += writeblob(fd, globls, strtab, n->expr.args[j]);
            }
        if (!found)
            sz += writepad(fd, size(dcl[i]));
    }
    end = alignto(sz, t);
    sz += writepad(fd, end - sz);
    return sz;
}

static size_t writeblob(FILE *fd, Htab *globls, Htab *strtab, Node *n)
{
    size_t i, sz;

    switch(exprop(n)) {
        case Otup:
        case Oarr:
            sz = 0;
            for (i = 0; i < n->expr.nargs; i++)
                sz += writeblob(fd, globls, strtab, n->expr.args[i]);
            break;
        case Ostruct:
            sz = writestruct(fd, globls, strtab, n);
            break;
        case Olit:
            sz = writelit(fd, strtab, n->expr.args[0], exprtype(n));
            break;
        case Oslice:
            sz = writeslice(fd, globls, strtab, n);
            break;
        default:
            dump(n, stdout);
            die("Nonliteral initializer for global");
            break;
    }
    return sz;
}

void genblob(FILE *fd, Node *blob, Htab *globls, Htab *strtab)
{
    char *lbl;

    /* lits and such also get wrapped in decls */
    assert(blob->type == Ndecl);

    lbl = htget(globls, blob);
    if (blob->decl.vis != Visintern)
        fprintf(fd, ".globl %s\n", lbl);
    fprintf(fd, "%s:\n", lbl);
    if (blob->decl.init)
        writeblob(fd, globls, strtab, blob->decl.init);
    else
        writepad(fd, size(blob));
}

/* genasm requires all nodes in 'nl' to map cleanly to operations that are
 * natively supported, as promised in the output of reduce().  No 64-bit
 * operations on x32, no structures, and so on. */
void genasm(FILE *fd, Func *fn, Htab *globls, Htab *strtab)
{
    Isel is = {0,};
    size_t i, j;
    char buf[128];

    is.reglocs = mkht(varhash, vareq);
    is.stkoff = fn->stkoff;
    is.globls = globls;
    is.ret = fn->ret;
    is.cfg = fn->cfg;
    /* ensure that all physical registers have a loc created, so we
     * don't get any surprises referring to them in the allocator */
    for (i = 0; i < Nreg; i++)
        locphysreg(i);

    for (i = 0; i < fn->cfg->nbb; i++)
        lappend(&is.bb, &is.nbb, mkasmbb(fn->cfg->bb[i]));

    is.curbb = is.bb[0];
    prologue(&is, fn->stksz);
    for (j = 0; j < fn->cfg->nbb - 1; j++) {
        is.curbb = is.bb[j];
        for (i = 0; i < fn->cfg->bb[j]->nnl; i++) {
            /* put in a comment that says where this line comes from */
            snprintf(buf, sizeof buf, "\n\t# bb = %zd, bbidx = %zd, %s:%d",
                     j, i, file->file.name, fn->cfg->bb[j]->nl[i]->line);
            g(&is, Ilbl, locstrlbl(buf), NULL);
            isel(&is, fn->cfg->bb[j]->nl[i]);
        }
    }
    is.curbb = is.bb[is.nbb - 1];
    epilogue(&is);
    regalloc(&is);

    if (debugopt['i'])
        writeasm(stdout, &is, fn);
    writeasm(fd, &is, fn);
}

void genstrings(FILE *fd, Htab *strtab)
{
    void **k;
    size_t i, nk;

    k = htkeys(strtab, &nk);
    for (i = 0; i < nk; i++) {
        fprintf(fd, "%s:\n", (char*)htget(strtab, k[i]));
        writebytes(fd, k[i], strlen(k[i]));
    }
}
