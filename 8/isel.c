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
#include "gen.h"
#include "asm.h"

/* instruction selection state */
typedef struct Isel Isel;
struct Isel {
    Insn **il;
    size_t ni;
    Node *ret;
    Htab *locs; /* Node => int stkoff */

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
    [Oeq] = {Itest, Ijnz, Isetnz},
    [One] = {Itest, Ijz, Isetz},
    [Ogt] = {Icmp, Ijg, Isetgt},
    [Oge] = {Icmp, Ijge, Isetge},
    [Olt] = {Icmp, Ijl, Isetlt},
    [Ole] = {Icmp, Ijle, Isetle}
};


/* forward decls */
Loc selexpr(Isel *s, Node *n);


Loc *loclbl(Loc *l, Node *lbl)
{
    assert(lbl->type = Nlbl);
    l->type = Loclbl;
    l->mode = ModeL;
    l->lbl = strdup(lbl->lbl.name);
    return l;
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
    return l;
}

Loc *locmeml(Loc *l, char *disp, Reg base, Reg idx, Mode mode)
{
    l->type = Locmem;
    l->mode = mode;
    l->mem.lbldisp = strdup(disp);
    l->mem.base = base;
    l->mem.idx = idx;
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
            if (!hthas(s->locs, (void*)n->expr.did))
                die("%s(%ld) not found", declname(n->expr.args[0]), n->expr.did);
            stkoff = (size_t)htget(s->locs, (void*)n->expr.did);
            locmem(&l, stkoff, Resp, Rnone, ModeL);
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
    printf("Got reg %s\n", regnames[l.reg]);

    return l;
}

void freereg(Isel *s, Reg r)
{
    int i;
    return;
    printf("Freed reg %s\n", regnames[r]);
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

Loc selexpr(Isel *s, Node *n)
{
    Loc a, b, c, r, t;
    Node **args;

    args = n->expr.args;
    r = (Loc){Locnone, };
    switch (exprop(n)) {
        case Oadd:      r = binop(s, Iadd, args[0], args[1]); break;
        case Osub:      r = binop(s, Isub, args[0], args[1]); break;

        case Omul:      die("Unimplemented op %s", opstr(exprop(n))); break;
        case Odiv:      die("Unimplemented op %s", opstr(exprop(n))); break;
        case Omod:      die("Unimplemented op %s", opstr(exprop(n))); break;
        case Oneg:      die("Unimplemented op %s", opstr(exprop(n))); break;

        case Obor:      r = binop(s, Ior,  args[0], args[1]); break;
        case Oband:     r = binop(s, Iand, args[0], args[1]); break;
        case Obxor:     r = binop(s, Ixor, args[0], args[1]); break;
        case Obsl:      die("Unimplemented op %s", opstr(exprop(n))); break;
        case Obsr:      die("Unimplemented op %s", opstr(exprop(n))); break;
        case Obnot:     die("Unimplemented op %s", opstr(exprop(n))); break;

        case Oderef:    die("Unimplemented op %s", opstr(exprop(n))); break;
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
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            b = inri(s, b);
            stor(s, &b, &a);
            r = b;
            break;
        case Oload: /* mem -> reg */
            t = selexpr(s, args[0]);
            b = getreg(s, t.mode);
            if (t.type == Locreg)
                locmem(&a, 0, t.reg, Rnone, t.mode);
            else
                a = t;
            /* load() doesn't always do the mov */
            g(s, Imov, &a, &b, NULL);
            r = b;
            break;

        case Ocall: die("Unimplemented op %s", opstr(exprop(n))); break;
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

        /* These operators should never show up in the reduced trees,
         * since they should have been replaced with more primitive
         * expressions by now */
        case Obad: case Oret: case Opreinc: case Opostinc: case Opredec:
        case Opostdec: case Olor: case Oland: case Oaddeq:
        case Osubeq: case Omuleq: case Odiveq: case Omodeq: case Oboreq:
        case Obandeq: case Obxoreq: case Obsleq: case Obsreq: case Omemb:
        case Oslice: case Oidx: case Osize: case Numops:
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

static void writeasm(Fn *fn, Isel *is, FILE *fd)
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
void genasm(Fn *fn)
{
    struct Isel is = {0,};
    int i;
    FILE *fd;

    is.locs = fn->locs;
    is.ret = fn->ret;

    prologue(&is, fn->stksz);
    for (i = 0; i < fn->nn; i++) {
        bzero(is.rtaken, sizeof is.rtaken);
        isel(&is, fn->nl[i]);
    }
    epilogue(&is);

    if (debug)
      writeasm(fn, &is, stdout);

    fd = fopen("a.s", "w");
    writeasm(fn, &is, fd);
    fclose(fd);
}
