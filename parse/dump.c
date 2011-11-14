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

static void dumpsym(Sym *s, FILE *fd, int depth)
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

static void dumpnode(Node *n, FILE *fd, int depth)
{
    int i;


    indent(fd, depth);
    if (!n) {
        fprintf(fd, "Nil\n");
        return;
    }
    fprintf(fd, "%s", nodestr(n->type));
    switch(n->type) {
        case Nfile:
            fprintf(fd, "(name = %s)\n", n->file.name);
            for (i = 0; i < n->file.nuses; i++)
                dumpnode(n->file.uses[i], fd, depth + 1);
            for (i = 0; i < n->file.nstmts; i++)
                dumpnode(n->file.stmts[i], fd, depth + 1);
            break;
        case Ndecl:
            fprintf(fd, "\n");
            dumpsym(n->decl.sym, fd, depth + 1);
            dumpnode(n->decl.init, fd, depth + 1);
            break;
        case Nblock:
            fprintf(fd, "\n");
            for (i = 0; i < n->block.nstmts; i++)
                dumpnode(n->block.stmts[i], fd, depth+1);
            break;
        case Nifstmt:
            fprintf(fd, "\n");
            dumpnode(n->ifstmt.cond, fd, depth+1);
            dumpnode(n->ifstmt.iftrue, fd, depth+1);
            dumpnode(n->ifstmt.iffalse, fd, depth+1);
            break;
        case Nloopstmt:
            dumpnode(n->loopstmt.init, fd, depth+1);
            dumpnode(n->loopstmt.cond, fd, depth+1);
            dumpnode(n->loopstmt.step, fd, depth+1);
            dumpnode(n->loopstmt.body, fd, depth+1);
            break;
        case Nuse:
            fprintf(fd, " (name = %s, islocal = %d)\n", n->use.name, n->use.islocal);
            break;
        case Nexpr:
            fprintf(fd, " (op = %s, flags = %d)\n", opstr(n->expr.op), n->expr.isconst);
            for (i = 0; i < n->expr.nargs; i++)
                dumpnode(n->expr.args[i], fd, depth+1);
            break;
        case Nlit:
            switch (n->lit.littype) {
                case Lchr:      fprintf(fd, " Lchr %c\n", n->lit.chrval); break;
                case Lbool:     fprintf(fd, " Lbool %s\n", n->lit.boolval ? "true" : "false"); break;
                case Lint:      fprintf(fd, " Lint %llu\n", n->lit.intval); break;
                case Lflt:      fprintf(fd, " Lflt %lf\n", n->lit.fltval); break;
                case Lfunc:
                    fprintf(fd, " Lfunc\n");
                    dumpnode(n->lit.fnval, fd, depth+1);
                    break;
                case Larray:
                    fprintf(fd, " Larray\n");
                    dumpnode(n->lit.arrval, fd, depth+1);
                    break;
                default: die("Bad literal type"); break;
            }
            break;
        case Nfunc:
            fprintf(fd, " (args =\n");
            for (i = 0; i < n->func.nargs; i++)
                dumpnode(n->func.args[i], fd, depth+1);
            indent(fd, depth);
            fprintf(fd, ")\n");
            dumpnode(n->func.body, fd, depth+1);
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
    dumpnode(n, fd, 0);
}
