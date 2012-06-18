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

/* string tables */
char *insnfmts[] = {
#define Insn(val, fmt, use, def) fmt,
#include "insns.def"
#undef Insn
};

char modenames[] = {
  [ModeB] = 'b',
  [ModeS] = 's',
  [ModeL] = 'l',
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
    [Ogt] = {Icmp, Ijg, Isetgt},
    [Oge] = {Icmp, Ijge, Isetge},
    [Olt] = {Icmp, Ijl, Isetlt},
    [Ole] = {Icmp, Ijle, Isetle}
};


static Loc *loc(Isel *s, Node *n)
{
    Loc *l;
    Node *v;
    size_t stkoff;

    switch (exprop(n)) {
        case Ovar:
            if (hthas(s->locs, n)) {
                stkoff = (size_t)htget(s->locs, n);
                l = locmem(-stkoff, locphysreg(Rebp), NULL, ModeL);
            } else if (hthas(s->globls, n)) {
                l = locstrlbl(htget(s->globls, n));
            } else {
                die("%s (id=%ld) not found", namestr(n->expr.args[0]), n->expr.did);
            }
            break;
        case Olit:
            v = n->expr.args[0];
            switch (v->lit.littype) {
                case Lchr:      l = loclit(v->lit.chrval); break;
                case Lbool:     l = loclit(v->lit.boolval); break;
                case Lint:      l = loclit(v->lit.intval); break;
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

static Mode mode(Node *n)
{
    switch (size(n)) {
        case 1: return ModeB; break;
        case 2: return ModeS; break;
        case 4: return ModeL; break;
    }
    dump(n, stdout);
    die("No mode for node");
    return ModeNone;
}

static Loc *coreg(Reg r, Mode m)
{
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

    assert(crtab[r][m] != Rnone);
    return locphysreg(crtab[r][m]);
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

static void g(Isel *s, AsmOp op, ...)
{
    va_list ap;
    Insn *i;

    va_start(ap, op);
    i = mkinsnv(op, ap);
    va_end(ap);
    lappend(&s->curbb->il, &s->curbb->ni, i);
}

static void load(Isel *s, Loc *a, Loc *b)
{
    Loc *l;
    assert(b->type == Locreg);
    if (a->type == Locreg)
        locmem(0, b, Rnone, a->mode);
    else
        l = a;
    g(s, Imov, l, b, NULL);
}

static void stor(Isel *s, Loc *a, Loc *b)
{
    Loc *l;

    assert(a->type == Locreg || a->type == Loclit);
    if (b->type == Locreg)
        locmem(0, b, Rnone, b->mode);
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
    l1 = loclbl(args[1]); /* if true */
    l2 = loclbl(args[2]); /* if false */

    g(s, cond, a, b, NULL);
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
 *        reg||const))
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
    if (exprop(e) == Oaddr) {
        l = selexpr(s, args[0]);
    } else if (exprop(e) == Oadd) {
        b = selexpr(s, args[0]);
        if (ismergablemul(args[1], &scale))
            o = selexpr(s, args[1]->expr.args[0]);
        else
            o = selexpr(s, args[1]);

        if (b->type != Locreg)
            b = inr(s, b);
        if (o->type == Loclit) {
            l = locmem(scale*o->lit, b, Rnone, m);
        } else if (o->type == Locreg) {
            b = inr(s, b);
            l = locmems(0, b, o, scale, m);
        }
    } else {
        l = selexpr(s, e);
        if (l->type == Locreg)
            l = locmem(0, l, Rnone, m);
    }
    return l;
}

static Loc *gencall(Isel *s, Node *n)
{
    int argsz, argoff;
    size_t i;
    Loc *eax, *esp;       /* hard-coded registers */
    Loc *stkbump;        /* calculated stack offset */
    Loc *dst, *arg, *fn;   /* values we reduced */

    esp = locphysreg(Resp);
    eax = locphysreg(Reax);
    argsz = 0;
    /* Have to calculate the amount to bump the stack
     * pointer by in one pass first, otherwise if we push
     * one at a time, we evaluate the args in reverse order.
     * Not good.
     *
     * We skip the first operand, since it's the function itself */
    for (i = 1; i < n->expr.nargs; i++)
        argsz += size(n->expr.args[i]);
    stkbump = loclit(argsz);
    if (argsz)
        g(s, Isub, stkbump, esp, NULL);

    /* Now, we can evaluate the arguments */
    argoff = 0;
    for (i = 1; i < n->expr.nargs; i++) {
        arg = selexpr(s, n->expr.args[i]);
        arg = inri(s, arg);
        dst = locmem(argoff, esp, NULL, arg->mode);
        stor(s, arg, dst);
        argoff += size(n->expr.args[i]);
    }
    fn = selexpr(s, n->expr.args[0]);
    g(s, Icall, fn, NULL);
    if (argsz)
        g(s, Iadd, stkbump, esp, NULL);
    return eax;
}

static void blit(Isel *s, Loc *a, Loc *b, int sz)
{
    int i;
    Loc *sp, *dp; /* pointers to src, dst */
    Loc *tmp, *src, *dst; /* source memory, dst memory */

    sp = inr(s, a);
    dp = inr(s, b);

    /* Slightly funny loop condition: We might have trailing bytes
     * that we can't blit word-wise. */
    tmp = locreg(ModeL);
    for (i = 0; i < sz/4; i++) {
        src = locmem(i, sp, NULL, ModeL);
        dst = locmem(i, dp, NULL, ModeL);
        g(s, Imov, src, tmp, NULL);
        g(s, Imov, tmp, dst, NULL);
    }
    /* now, the trailing bytes */
    tmp = locreg(ModeB);
    for (; i < sz%4; i++) {
        src = locmem(i, sp, NULL, ModeB);
        dst = locmem(i, dp, NULL, ModeB);
        g(s, Imov, src, tmp, NULL);
        g(s, Imov, tmp, dst, NULL);
    }
}

Loc *selexpr(Isel *s, Node *n)
{
    Loc *a, *b, *c, *d, *r;
    Loc *eax, *edx, *cl; /* x86 wanst some hard-coded regs */
    Node **args;

    args = n->expr.args;
    eax = locphysreg(Reax);
    edx = locphysreg(Redx);
    cl = locphysreg(Rcl);
    switch (exprop(n)) {
        case Oadd:      r = binop(s, Iadd, args[0], args[1]); break;
        case Osub:      r = binop(s, Isub, args[0], args[1]); break;
        case Obor:      r = binop(s, Ior,  args[0], args[1]); break;
        case Oband:     r = binop(s, Iand, args[0], args[1]); break;
        case Obxor:     r = binop(s, Ixor, args[0], args[1]); break;

        case Omul:
            /* these get clobbered by the mul insn */
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inr(s, b);
            c = coreg(Reax, mode(n));
            r = locreg(a->mode);
            g(s, Imov, a, c, NULL);
            g(s, Imul, b, NULL);
            g(s, Imov, eax, r, NULL);
            break;
        case Odiv:
        case Omod:
            /* these get clobbered by the div insn */
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inr(s, b);
            c = coreg(Reax, mode(n));
            r = locreg(a->mode);
            g(s, Imov, a, c, NULL);
            g(s, Ixor, edx, edx, NULL);
            g(s, Idiv, b, NULL);
            if (exprop(n) == Odiv)
                d = eax;
            else
                d = edx;
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
                g(s, Ishl, cl, a, NULL);
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
            r = locreg(a->mode);
            c = locmem(0, a, Rnone, a->mode);
            g(s, Imov, c, r, NULL);
            break;

        case Oaddr:
            a = selexpr(s, args[0]);
            r = locreg(ModeL);
            g(s, Ilea, a, r, NULL);
            break;

        case Olnot:
            a = selexpr(s, args[0]);
            b = locreg(ModeB);
            r = locreg(mode(n));
            g(s, reloptab[exprop(n)].test, a, a, NULL);
            g(s, reloptab[exprop(n)].getflag, b, NULL);
            g(s, Imovz, b, r, NULL);
            break;

        case Oeq: case One: case Ogt: case Oge: case Olt: case Ole:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inr(s, b);
            c = locreg(ModeB);
            r = locreg(mode(n));
            g(s, reloptab[exprop(n)].test, a, b, NULL);
            g(s, reloptab[exprop(n)].getflag, c, NULL);
            g(s, Imovz, c, r, NULL);
            return r;

        case Oasn:  /* relabel */
            die("Unimplemented op %s", opstr(exprop(n)));
            break;
        case Ostor: /* reg -> mem */
            b = selexpr(s, args[1]);
            a = memloc(s, args[0], mode(args[0]));
            b = inri(s, b);
            g(s, Imov, b, a, NULL);
            r = b;
            break;
        case Oload: /* mem -> reg */
            b = memloc(s, args[0], mode(n));
            r = locreg(mode(n));
            g(s, Imov, b, r, NULL);
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
            b = selexpr(s, args[0]);
            a = selexpr(s, args[1]);
            blit(s, a, b, args[2]->expr.args[0]->lit.intval);
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
        case Oslbase: case Osllen: case Ocast:
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
            if (l->reg.colour == Rnone)
                fprintf(fd, "%%P.%ld", l->reg.id);
            else
                fprintf(fd, "%s", regnames[l->reg.colour]);
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
            fprintf(fd, "(");
            locprint(fd, l->mem.base);
            if (l->mem.idx) {
                fprintf(fd, ",");
                locprint(fd, l->mem.idx);
            }
            if (l->mem.scale > 1)
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
                goto done; /* skip the final p++ */
            case 'r':
            case 'm':
            case 'l':
            case 'x':
            case 'v':
                locprint(fd, insn->args[i]);
                i++;
                break;
            case 't':
                modeidx = 0;
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
    Loc *lbl;

    switch (n->type) {
        case Nlbl:
            g(s, Ilbl, lbl = loclbl(n), NULL);
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

static void prologue(Isel *s, size_t sz)
{
    Loc *esp;
    Loc *ebp;
    Loc *stksz;

    esp = locphysreg(Resp);
    ebp = locphysreg(Rebp);
    stksz = loclit(sz);
    g(s, Ipush, ebp, NULL);
    g(s, Imov, esp, ebp, NULL);
    g(s, Isub, stksz, esp, NULL);
    s->stksz = stksz; /* need to update if we spill */
}

static void epilogue(Isel *s)
{
    Loc *esp, *ebp, *eax;
    Loc *ret;

    esp = locphysreg(Resp);
    ebp = locphysreg(Rebp);
    eax = locphysreg(Reax);
    if (s->ret) {
        ret = loc(s, s->ret);
        g(s, Imov, ret, eax, NULL);
    }
    g(s, Imov, ebp, esp, NULL);
    g(s, Ipop, ebp, NULL);
    g(s, Iret, NULL);
}

static void writeasm(FILE *fd, Isel *s, Func *fn)
{
    size_t i, j;

    if (fn->isglobl)
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

void writeblob(FILE *fd, char *p, size_t sz)
{
    size_t i;

    for (i = 0; i < sz; i++) {
        if (i % 60 == 0)
            fprintf(fd, "\t.string \"");
        if (isprint(p[i]))
            fprintf(fd, "%c", p[i]);
        else
            fprintf(fd, "\\%x", p[i]);
        if (i % 60 == 59 || i == sz - 1)
            fprintf(fd, "\"\n");
    }
}

void writelit(FILE *fd, Node *v)
{
    char lbl[128];
    switch (v->lit.littype) {
        case Lbool:     fprintf(fd, "\t.long %d\n", v->lit.boolval);     break;
        case Lchr:      fprintf(fd, "\t.byte %d\n",  v->lit.chrval);     break;
        case Lint:      fprintf(fd, "\t.long %lld\n", v->lit.intval);    break;
        case Lflt:      fprintf(fd, "\t.double %f\n", v->lit.fltval);    break;
        case Lstr:      fprintf(fd, "\t.long %s\n", genlblstr(lbl, 128));
                        fprintf(fd, "\t.long %zd\n", strlen(v->lit.strval));
                        fprintf(fd, "%s:\n", lbl);
                        writeblob(fd, v->lit.strval, strlen(v->lit.strval));
                        break;
        case Larray:
        case Lfunc:
                        die("Generating this shit ain't ready yet ");
    }
}

void genblob(FILE *fd, Node *blob, Htab *globls)
{
    char *lbl;

    /* lits and such also get wrapped in decls */
    assert(blob->type == Ndecl);
    assert(blob->decl.init != NULL);

    lbl = htget(globls, blob);
    fprintf(fd, "%s:\n", lbl);
    if (exprop(blob->decl.init) != Olit)
        die("Nonliteral initializer for global");
    writelit(fd, blob->decl.init->expr.args[0]);
}

/* genasm requires all nodes in 'nl' to map cleanly to operations that are
 * natively supported, as promised in the output of reduce().  No 64-bit
 * operations on x32, no structures, and so on. */
void genasm(FILE *fd, Func *fn, Htab *globls)
{
    Isel is = {0,};
    size_t i, j;
    char buf[128];

    is.locs = fn->locs;
    is.globls = globls;
    is.ret = fn->ret;
    is.cfg = fn->cfg;

    for (i = 0; i < fn->cfg->nbb; i++)
        lappend(&is.bb, &is.nbb, mkasmbb(fn->cfg->bb[i]));

    is.curbb = is.bb[0];
    prologue(&is, fn->stksz);
    for (j = 0; j < fn->cfg->nbb; j++) {
        is.curbb = is.bb[j];
        for (i = 0; i < fn->cfg->bb[j]->nnl; i++) {
            /* put in a comment that says where this line comes from */
            snprintf(buf, sizeof buf, "\n\t# bb = %zd, bbidx = %zd, line=%d",
                     j, i, fn->cfg->bb[j]->nl[i]->line);
            isel(&is, fn->cfg->bb[j]->nl[i]);
            g(&is, Ilbl, locstrlbl(buf), NULL);
        }
    }
    is.curbb = is.bb[is.nbb - 1];
    epilogue(&is);
    regalloc(&is);

    if (debug)
        writeasm(stdout, &is, fn);
    writeasm(fd, &is, fn);
}

