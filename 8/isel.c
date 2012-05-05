#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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
#define Reg(r, name) name,
#include "regs.def"
#undef Reg
};

char *insnfmts[] = {
#define Insn(val, fmt, attr) fmt,
#include "insns.def"
#undef Insn
};

void selexpr(Node *n)
{
    switch (exprop(n)) {
        case Obad:
        case Oadd:
        case Osub:
        case Omul:
        case Odiv:
        case Omod:
        case Oneg:
        case Obor:
        case Oband:
        case Obxor:
        case Obsl:
        case Obsr:
        case Obnot:
        case Opreinc:
        case Opostinc:
        case Opredec:
        case Opostdec:
        case Oaddr:
        case Oderef:
        case Olor:
        case Oland:
        case Olnot:
        case Oeq:
        case One:
        case Ogt:
        case Oge:
        case Olt:
        case Ole:
        case Oasn:
        case Oaddeq:
        case Osubeq:
        case Omuleq:
        case Odiveq:
        case Omodeq:
        case Oboreq:
        case Obandeq:
        case Obxoreq:
        case Obsleq:
        case Obsreq:
        case Oidx:
        case Oslice:
        case Omemb:
        case Osize:
        case Ocall:
        case Ocast:
        case Oret:
        case Ojmp:
        case Ocjmp:
        case Ovar:
        case Olit:
        case Olbl:
            break;
    }
}

void ilbl(Isel *s, char *name)
{
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
            break;
        case Loclit:
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
            default:
                if (isdigit(*p))
                    modeidx = strtol(p, &p, 10);
                else
                    modeidx = 0;
                modeprint(fd, &insn->args[modeidx]);
                break;
        }
    }
done:
    return;
}

void isel(Node *n)
{
    struct Isel is = {0,};
    switch (n->type) {
        case Nlbl:
            ilbl(&is, n->lbl.name);
            break;
        case Nexpr:
            selexpr(n);
            break;
        case Ndecl:
            break;
        default:
            die("Bad node type in isel()");
            break;
    }
}
