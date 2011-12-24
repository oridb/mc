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

static void indent(FILE *fd, int depth)
{
    int i;
    for (i = 0; i < 4*depth; i++)
        fprintf(fd, " ");
}

static void outsym(Sym *s, FILE *fd, int depth)
{
    int i;
    char buf[1024];

    indent(fd, depth);
    fprintf(fd, "Sym ");
    for (i = 0; i < s->name->name.nparts; i++) {
        fprintf(fd, "%s", s->name->name.parts[i]);
        if (i != s->name->name.nparts - 1)
            fprintf(fd, ".");
    }
    fprintf(fd, " : %s\n", tyfmt(buf, 1024, s->type));
}

void dumpsym(Sym *s, FILE *fd)
{
    outsym(s, fd, 0);
}

static void outstab(Stab *st, FILE *fd, int depth)
{
    int i;

    indent(fd, depth);
    fprintf(fd, "Stab %p (super = %p)\n", st, st ? st->super : NULL);
    if (!st)
        return;
    for (i = 0; i < st->ntypes; i++) {
        indent(fd, depth + 1);
        fprintf(fd, "T ");
        /* already indented */
        outsym(st->types[i], fd, 0);
    }
    for (i = 0; i < st->nsyms; i++) {
        indent(fd, depth + 1);
        fprintf(fd, "V ");
        /* already indented */
        outsym(st->syms[i], fd, 0);
    }
}

void dumpstab(Stab *st, FILE *fd)
{
    outstab(st, fd, 0);
}

static void outnode(Node *n, FILE *fd, int depth)
{
    int i;
    char *ty;

    indent(fd, depth);
    if (!n) {
        fprintf(fd, "Nil\n");
        return;
    }
    fprintf(fd, "%s", nodestr(n->type));
    switch(n->type) {
        case Nfile:
            fprintf(fd, "(name = %s)\n", n->file.name);
            indent(fd, depth + 1);
            fprintf(fd, "Globls:\n");
            outstab(n->file.globls, fd, depth + 2);
            indent(fd, depth + 1);
            fprintf(fd, "Exports:\n");
            outstab(n->file.exports, fd, depth + 2);
            for (i = 0; i < n->file.nuses; i++)
                outnode(n->file.uses[i], fd, depth + 1);
            for (i = 0; i < n->file.nstmts; i++)
                outnode(n->file.stmts[i], fd, depth + 1);
            break;
        case Ndecl:
            fprintf(fd, "\n");
            outsym(n->decl.sym, fd, depth + 1);
            outnode(n->decl.init, fd, depth + 1);
            break;
        case Nblock:
            fprintf(fd, "\n");
            outstab(n->block.scope, fd, depth + 1);
            for (i = 0; i < n->block.nstmts; i++)
                outnode(n->block.stmts[i], fd, depth+1);
            break;
        case Nifstmt:
            fprintf(fd, "\n");
            outnode(n->ifstmt.cond, fd, depth+1);
            outnode(n->ifstmt.iftrue, fd, depth+1);
            outnode(n->ifstmt.iffalse, fd, depth+1);
            break;
        case Nloopstmt:
            outnode(n->loopstmt.init, fd, depth+1);
            outnode(n->loopstmt.cond, fd, depth+1);
            outnode(n->loopstmt.step, fd, depth+1);
            outnode(n->loopstmt.body, fd, depth+1);
            break;
        case Nuse:
            fprintf(fd, " (name = %s, islocal = %d)\n", n->use.name, n->use.islocal);
            break;
        case Nexpr:
            ty = tystr(n->expr.type);
            fprintf(fd, " (type = %s, op = %s, flags = %d)\n", ty, opstr(n->expr.op), n->expr.isconst);
            free(ty);
            for (i = 0; i < n->expr.nargs; i++)
                outnode(n->expr.args[i], fd, depth+1);
            break;
        case Nlit:
            switch (n->lit.littype) {
                case Lchr:      fprintf(fd, " Lchr %c\n", n->lit.chrval); break;
                case Lbool:     fprintf(fd, " Lbool %s\n", n->lit.boolval ? "true" : "false"); break;
                case Lint:      fprintf(fd, " Lint %llu\n", n->lit.intval); break;
                case Lflt:      fprintf(fd, " Lflt %lf\n", n->lit.fltval); break;
                case Lfunc:
                    fprintf(fd, " Lfunc\n");
                    outnode(n->lit.fnval, fd, depth+1);
                    break;
                case Larray:
                    fprintf(fd, " Larray\n");
                    outnode(n->lit.arrval, fd, depth+1);
                    break;
                default: die("Bad literal type"); break;
            }
            break;
        case Nfunc:
            fprintf(fd, " (args =\n");
            outstab(n->func.scope, fd, depth + 1);
            for (i = 0; i < n->func.nargs; i++)
                outnode(n->func.args[i], fd, depth+1);
            indent(fd, depth);
            fprintf(fd, ")\n");
            outnode(n->func.body, fd, depth+1);
            break;
        case Nlbl:
            fprintf(fd, "(lbl = %s)\n", n->lbl.name);
            break;
        case Nname:
            fprintf(fd, "(");
            for (i = 0; i < n->name.nparts; i++) {
                if (i != 0)
                    fprintf(fd, ".");
                fprintf(fd, "%s", n->name.parts[i]);
            }
            fprintf(fd, ")\n");
            break;
    }
}

void dump(Node *n, FILE *fd)
{
    outnode(n, fd, 0);
}
