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

typedef struct Isel Isel;
struct Isel {
    Insn **il;
    size_t ni;
};

char *regnames[] = {
#define Reg(r, name, mode) name,
#include "regs.def"
#undef Reg
};

Mode regmodes[] = {
#define Reg(r, name, mode) mode,
#include "regs.def"
#undef Reg
};

char *insnfmts[] = {
#define Insn(val, fmt, attr) fmt,
#include "insns.def"
#undef Insn
};


Loc *loclbl(Loc *l, char *lbl)
{
    l->type = Loclbl;
    l->mode = ModeL;
    l->lbl = strdup(lbl);
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
    switch (exprop(n)) {
        case Ovar:
            locmem(&l, 0, Reax, Rnone, ModeL);
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

Loc getreg(Isel *s, Mode m)
{
    Loc l;
    locreg(&l, Reax);
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

Loc selexpr(Isel *s, Node *n)
{
    Loc a, b, r;
    Node **args;

    args = n->expr.args;
    r = (Loc){Locnone, };
    switch (exprop(n)) {
        case Oadd:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            g(s, Iadd, &b, &a, NULL);
            r = b;
            break;

        case Osub:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            g(s, Iadd, &b, &a, NULL);
            r = b;
            break;

        case Omul: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Odiv: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Omod: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Oneg: die("Unimplemented op %s", opstr(exprop(n))); break;

        case Obor: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Oband: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Obxor: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Obsl: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Obsr: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Obnot: die("Unimplemented op %s", opstr(exprop(n))); break;

        case Oaddr: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Oderef: die("Unimplemented op %s", opstr(exprop(n))); break;

        case Oeq: die("Unimplemented op %s", opstr(exprop(n))); break;
        case One: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Ogt: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Oge: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Olt: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Ole: die("Unimplemented op %s", opstr(exprop(n))); break;

        case Oasn:
            a = selexpr(s, args[0]);
            b = selexpr(s, args[1]);
            g(s, Imov, &b, &a, NULL);
            r = b;
            break;

        case Oidx: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Oslice: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Osize: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Ocall: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Ocast: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Ojmp:
            g(s, Ijmp, loclbl(&a, args[0]->lbl.name), NULL);
            break;
        case Ocjmp: die("Unimplemented op %s", opstr(exprop(n))); break;
        case Olit:
        case Ovar:
            a = loc(s, n);
            b = getreg(s, mode(args[0]));
            g(s, Imov, &a, &b, NULL);
            r = b;
            break;
        case Olbl: die("Unimplemented op %s", opstr(exprop(n))); break;

        case Obad: case Oret: case Opreinc: case Opostinc: case Opredec:
        case Opostdec: case Olor: case Oland: case Olnot: case Oaddeq:
        case Osubeq: case Omuleq: case Odiveq: case Omodeq: case Oboreq:
        case Obandeq: case Obxoreq: case Obsleq: case Obsreq: case Omemb:
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
    char mode[] = {'b', 's', 'l', 'q'};
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
            g(s, Ilbl, loclbl(&lbl, n->lbl.name), NULL);
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

void prologue(Isel *s)
{
    Loc resp;
    Loc rebp;
    Loc stksz;

    locreg(&resp, Resp);
    locreg(&rebp, Rebp);
    loclit(&stksz, 16);
    g(s, Ipush, &rebp, NULL);
    g(s, Imov, &resp, &rebp, NULL);
    g(s, Isub, &stksz, &resp, NULL);
}

void epilogue(Isel *s)
{
    Loc resp;
    Loc rebp;
    Loc stksz;

    locreg(&resp, Resp);
    locreg(&rebp, Rebp);
    loclit(&stksz, 16);
    g(s, Imov, &rebp, &resp, NULL);
    g(s, Ipop, &rebp, NULL);
}

void genasm(Node **nl, int nn)
{
    struct Isel is = {0,};
    int i;

    prologue(&is);
    for (i = 0; i < nn; i++)
        isel(&is, nl[i]);
    epilogue(&is);

    for (i = 0; i < is.ni; i++)
        iprintf(stdout, is.il[i]);
}
