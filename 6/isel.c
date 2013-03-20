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

#include "platform.h"

/* string tables */
char *insnfmts[] = {
#define Insn(val, fmt, use, def) fmt,
#include "insns.def"
#undef Insn
};

char modenames[] = {
  [ModeB] = 'b',
  [ModeW] = 'w',
  [ModeL] = 'l',
  [ModeQ] = 'q',
  [ModeF] = 'f',
  [ModeD] = 'd'
};

/* forward decls */
Loc *selexpr(Isel *s, Node *n);

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
    [Ogt] = {Icmp, Ijg, Isetg},
    [Oge] = {Icmp, Ijge, Isetge},
    [Olt] = {Icmp, Ijl, Isetl},
    [Ole] = {Icmp, Ijle, Isetle}
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

static Loc *loc(Isel *s, Node *n)
{
    ssize_t stkoff;
    Loc *l, *rip;
    Node *v;

    switch (exprop(n)) {
        case Ovar:
            if (hthas(s->stkoff, n)) {
                stkoff = (ssize_t)htget(s->stkoff, n);
                l = locmem(-stkoff, locphysreg(Rrbp), NULL, mode(n));
            } else if (hthas(s->globls, n)) {
                if (tybase(exprtype(n))->type == Tyfunc)
                    rip = NULL;
                else
                    rip = locphysreg(Rrip);
                l = locmeml(htget(s->globls, n), rip, NULL, mode(n));
            } else {
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

    n = 0;
    i = malloc(sizeof(Insn));
    i->op = op;
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
        g(s, Imovz, src, dst, NULL);
}

static void load(Isel *s, Loc *a, Loc *b)
{
    Loc *l;

    assert(b->type == Locreg);
    if (a->type == Locreg)
        l = locmem(0, b, Rnone, a->mode);
    else
        l = a;
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
    g(s, Imov, a, l, NULL);
}

/* ensures that a location is within a reg */
static Loc *inr(Isel *s, Loc *a)
{
    Loc *r;

    if (a->type == Locreg)
        return a;
    r = locreg(a->mode);
    load(s, a, r);
    return r;
}

/* ensures that a location is within a reg or an imm */
static Loc *inri(Isel *s, Loc *a)
{
    if (a->type == Locreg || a->type == Loclit)
        return a;
    else
        return inr(s, a);
}

/* ensures that a location is within a reg or an imm */
static Loc *inrm(Isel *s, Loc *a)
{
    if (a->type == Locreg || a->type == Locmem)
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
        a = inr(s, a);
    } else {
        cond = Itest;
        jmp = Ijnz;
        b = inr(s, selexpr(s, args[0])); /* cond */
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
    a = inr(s, a);
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
    AsmOp op;
    Loc *sp, *dp, *len; /* pointers to src, dst */

    if (sz % 8 == 0) {
        sz /= 8;
        op = Irepmovsq;
    } else if (sz % 4 == 0) {
        sz /= 4;
        op = Irepmovsl;
    } else if (sz % 2 == 0) {
        sz /= 2;
        op = Irepmovsw;
    } else {
        op = Irepmovsb;
    }

    len = loclit(sz, ModeQ);
    sp = inr(s, from);
    dp = inr(s, to);

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
    g(s, op, NULL);
}

static Loc *gencall(Isel *s, Node *n)
{
    Loc *src, *dst, *arg, *fn;  /* values we reduced */
    Loc *rax, *rsp, *ret;       /* hard-coded registers */
    Loc *stkbump;        /* calculated stack offset */
    int argsz, argoff;
    size_t i;

    rsp = locphysreg(Rrsp);
    if (tybase(exprtype(n))->type == Tyvoid) {
        rax = NULL;
        ret = NULL;
    } else if (stacktype(exprtype(n))) {
        rax = locphysreg(Rrax);
        ret = locreg(ModeQ);
    } else {
        rax = coreg(Rrax, mode(n));
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
    stkbump = loclit(argsz, ModeQ);
    if (argsz)
        g(s, Isub, stkbump, rsp, NULL);

    /* Now, we can evaluate the arguments */
    argoff = 0;
    for (i = 1; i < n->expr.nargs; i++) {
        arg = selexpr(s, n->expr.args[i]);
        argoff = align(argoff, min(size(n->expr.args[i]), Ptrsz));
        if (stacknode(n->expr.args[i])) {
            dst = locreg(ModeQ);
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
    fn = selexpr(s, n->expr.args[0]);
    if (fn->type == Loclbl || fn->type == Locmeml)
        g(s, Icall, fn, NULL);
    else
        g(s, Icallind, fn, NULL);
    if (argsz)
        g(s, Iadd, stkbump, rsp, NULL);
    if (rax)
        g(s, Imov, rax, ret, NULL);
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
        case Omul:      r = binop(s, Iimul, args[0], args[1]); break;
        case Odiv:
        case Omod:
            /* these get clobbered by the div insn */
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inr(s, b);
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
            r = inr(s, r);
            g(s, Ineg, r, NULL);
            break;

        case Obsl:
        case Obsr:
            a = inr(s, selexpr(s, args[0]));
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
            r = inrm(s, r);
            g(s, Inot, r, NULL);
            break;

        case Oderef:
            a = selexpr(s, args[0]);
            a = inr(s, a);
            r = locmem(0, a, Rnone, mode(n));
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
            a = selexpr(s, args[0]);
            b = locreg(ModeB);
            r = locreg(mode(n));
            g(s, reloptab[exprop(n)].test, a, a, NULL);
            g(s, reloptab[exprop(n)].getflag, b, NULL);
            movz(s, b, r);
            break;

        case Oeq: case One: case Ogt: case Oge: case Olt: case Ole:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            a = inr(s, a);
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
            r = loc(s, n);
            break;
        case Olbl:
            r = loclbl(args[0]);
            break;
        case Oblit:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            blit(s, a, b, 0, 0, args[2]->expr.args[0]->lit.intval);
            r = b;
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
            b = locreg(mode(n));
            movz(s, a, b);
            r = b;
            break;
        case Oswiden:
            a = selexpr(s, args[0]);
            a = inr(s, a);
            b = locreg(mode(n));
            g(s, Imovs, a, b, NULL);
            r = b;
            break;

        /* These operators should never show up in the reduced trees,
         * since they should have been replaced with more primitive
         * expressions by now */
        case Obad: case Oret: case Opreinc: case Opostinc: case Opredec:
        case Opostdec: case Olor: case Oland: case Oaddeq:
        case Osubeq: case Omuleq: case Odiveq: case Omodeq: case Oboreq:
        case Obandeq: case Obxoreq: case Obsleq: case Obsreq: case Omemb:
        case Oslice: case Oidx: case Osize: case Numops:
        case Oucon: case Ouget: case Otup: case Oarr:
        case Oslbase: case Osllen: case Ocast:
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
            assert(spec == 'r' || spec == 'v' || spec == 'x' || spec == 'u');
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
        case Imovz:
            if (insn->args[0]->mode == ModeL && insn->args[1]->mode == ModeQ) {
                if (insn->args[1]->reg.colour) {
                    insn->op = Imov;
                    insn->args[1] = coreg(insn->args[1]->reg.colour, ModeL);
                }
            }
            break;
        case Imov:
            if (insn->args[0]->type != Locreg || insn->args[1]->type != Locreg)
                break;
            if (insn->args[0]->reg.colour == Rnone || insn->args[1]->reg.colour == Rnone)
                break;
            /* if one reg is a subreg of another, we can just use the right
             * mode to move between them. */
            if (insn->args[0]->mode != insn->args[1]->mode)
                insn->args[0] = coreg(insn->args[0]->reg.colour, insn->args[1]->mode);
            /* moving a reg to itself is dumb. */
     //       if (insn->args[0]->reg.colour == insn->args[1]->reg.colour)
     //           return;
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
            case 'r': /* register */
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
                    fputc(modenames[insn->args[modeidx]->mode], fd);
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
    Rrcx,
    Rrdx,
    Rrbx,
    Rrsi,
    Rrdi,
    Rr8,
    Rr9,
    Rr10,
    Rr11,
    Rr12,
    Rr13,
    Rr14,
    Rr15
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

    if (fn->isexport || !strcmp(fn->name, Symprefix "main"))
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

static void writeblob(FILE *fd, char *p, size_t sz)
{
    size_t i;

    for (i = 0; i < sz; i++) {
        if (i % 60 == 0)
            fprintf(fd, "\t.ascii \"");
        if (isprint(p[i]))
            fprintf(fd, "%c", p[i]);
        else
            fprintf(fd, "\\%03o", (uint8_t)p[i] & 0xff);
        /* line wrapping for readability */
        if (i % 60 == 59 || i == sz - 1)
            fprintf(fd, "\"\n");
    }
}

static void writelit(FILE *fd, Node *v, size_t sz)
{
    char lbl[128];
    size_t i;
    char *intsz[] = {
        [1] = ".byte",
        [2] = ".short",
        [4] = ".long",
        [8] = ".quad"
    };

    assert(v->type == Nlit);
    switch (v->lit.littype) {
        case Lbool:     fprintf(fd, "\t.byte %d\n", v->lit.boolval);     break;
        case Lchr:      fprintf(fd, "\t.long %d\n",  v->lit.chrval);     break;
        case Lint:      fprintf(fd, "\t%s %lld\n", intsz[sz], v->lit.intval);    break;
        case Lflt:      fprintf(fd, "\t.double %f\n", v->lit.fltval);    break;
        case Lstr:      fprintf(fd, "\t.quad %s\n", genlblstr(lbl, 128));
                        fprintf(fd, "\t.quad %zd\n", strlen(v->lit.strval));
                        fprintf(fd, "%s:\n", lbl);
                        writeblob(fd, v->lit.strval, strlen(v->lit.strval));
                        break;
        case Lseq:
            for (i = 0; i < v->lit.nelt; i++)
                writelit(fd, v->lit.seqval[i]->expr.args[0], size(v->lit.seqval[i]));
            break;
        case Lfunc:
            die("Generating this shit ain't ready yet ");
            break;
        case Llbl:
            die("Can't generate literal labels, ffs. They're not data.");
            break;
    }
}

void genblob(FILE *fd, Node *blob, Htab *globls)
{
    size_t i;
    char *lbl;

    /* lits and such also get wrapped in decls */
    assert(blob->type == Ndecl);

    lbl = htget(globls, blob);
    if (blob->decl.isexport)
        fprintf(fd, ".globl %s\n", lbl);
    fprintf(fd, "%s:\n", lbl);
    if (blob->decl.init) {
        if (exprop(blob->decl.init) != Olit)
            die("Nonliteral initializer for global");
        writelit(fd, blob->decl.init->expr.args[0], size(blob));
    } else {
        for (i = 0; i < size(blob); i++)
            fprintf(fd, "\t.byte 0\n");
    }
}

/* genasm requires all nodes in 'nl' to map cleanly to operations that are
 * natively supported, as promised in the output of reduce().  No 64-bit
 * operations on x32, no structures, and so on. */
void genasm(FILE *fd, Func *fn, Htab *globls)
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
            snprintf(buf, sizeof buf, "\n\t# bb = %zd, bbidx = %zd, line=%d",
                     j, i, fn->cfg->bb[j]->nl[i]->line);
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
