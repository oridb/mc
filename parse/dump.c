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

static void dumpnode(Node *n, FILE *fd, int depth)
{
    int i;


    indent(fd, depth);
    fprintf(fd, "%s", nodestr(n->type));
    switch(n->type) {
        case Nfile:
            fprintf(fd, "(name = %s)\n", n->file.name);
            break;
        case Nblock:
            for (i = 0; i < n->block.nstmts; i++)
                dumpnode(n->block.stmts[i], fd, depth+1);
            break;
        case Nifstmt:
            dumpnode(n->ifstmt.cond, fd, depth+1);
            dumpnode(n->ifstmt.iftrue, fd, depth+1);
            dumpnode(n->ifstmt.iffalse, fd, depth+1);
            break;
        case Nloopstmt:
            dumpnode(n->loopstmt.init, fd, depth+1);
            dumpnode(n->loopstmt.cond, fd, depth+1);
            dumpnode(n->loopstmt.incr, fd, depth+1);
            dumpnode(n->loopstmt.body, fd, depth+1);
            break;
        case Nuse:
            fprintf(fd, " (name = %s, islocal = %d)\n", n->use.name, n->use.islocal);
            break;
        case Nexpr:
            fprintf(fd, " (op = %s, isconst = %d)\n", opstr(n->expr.op), n->expr.isconst);
            for (i = 0; i < n->expr.nargs; i++)
                dumpnode(n->expr.args[i], fd, depth+1);
            break;
        case Nlit:
            indent(fd, depth);
            switch (n->lit.littype) {
                case Lchr:      fprintf(fd, "Lchr %c\n", n->lit.chrval); break;
                case Lbool:     fprintf(fd, "Lbool %s\n", n->lit.boolval ? "true" : "false"); break;
                case Lint:      fprintf(fd, "Lint %ld\n", n->lit.intval); break;
                case Lflt:      fprintf(fd, "Lflt %lf\n", n->lit.fltval); break;
                /*
                case Lfunc:     fprintf("Lfunc %s\n", n->lit.chrval); break;
                case Larray:    fprintf("Larray %c\n", n->lit.chrval); break;
                */
                default: die("Bad literal type"); break;
            }
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
    dumpnode(n, fd, 0);
}
