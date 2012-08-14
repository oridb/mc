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
    for (i = 0; i < depth; i++)
        fprintf(fd, "    ");
}

/* outputs a fully qualified name */
static void outname(Node *n, FILE *fd)
{
    if (n->name.ns)
        fprintf(fd, "%s.", n->name.ns);
    fprintf(fd, "%s", n->name.name);
}

/* outputs a sym in a one-line short form (ie,
 * the initializer is not printed, and the node is not
 * expressed in indented tree. */
static void outsym(Node *s, FILE *fd, int depth)
{
    char buf[1024];

    indent(fd, depth);
    if (s->decl.isconst)
        fprintf(fd, "const ");
    else
        fprintf(fd, "var ");
    outname(s->decl.name, fd);
    fprintf(fd, " : %s\n", tyfmt(buf, 1024, s->decl.type));
}

void dumpsym(Node *s, FILE *fd)
{
    outsym(s, fd, 0);
}

/* Outputs a symbol table, and it's sub-tables
 * recursively, with a sigil describing the symbol
 * type, as follows:
 *      T       type
 *      S       symbol
 *      N       namespace
 *
 * Does not print captured variables.
 */
static void outstab(Stab *st, FILE *fd, int depth)
{
    size_t i, n;
    void **k;
    char *ty;

    indent(fd, depth);
    fprintf(fd, "Stab %p (super = %p, name=\"%s\")\n", st, st->super, namestr(st->name));
    if (!st)
        return;

    /* print types */
    k = htkeys(st->ty, &n);
    for (i = 0; i < n; i++) {
        indent(fd, depth + 1);
        fprintf(fd, "T ");
        /* already indented */
        outname(k[i], fd); 
        ty = tystr(gettype(st, k[i]));
        fprintf(fd, " = %s\n", ty);
        free(ty);
    }
    free(k);

    k = htkeys(st->dcl, &n);
    for (i = 0; i < n; i++) {
        indent(fd, depth + 1);
        fprintf(fd, "S ");
        /* already indented */
        outsym(getdcl(st, k[i]), fd, 0);
    }
    free(k);

    k = htkeys(st->ns, &n);
    for (i = 0; i < n; i++) {
        indent(fd, depth + 1);
        fprintf(fd, "N  %s\n", namestr(k[i]));
        outstab(getns(st, k[i]), fd, depth + 1);
    }
    free(k);
}

void dumpstab(Stab *st, FILE *fd)
{
    outstab(st, fd, 0);
}

/* Outputs a node in indented tree form. This is
 * not a full serialization, but mainly an aid for
 * understanding and debugging. */
static void outnode(Node *n, FILE *fd, int depth)
{
    size_t i;
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
            fprintf(fd, "(did = %zd, isconst = %d, isgeneric = %d, isextern = %d\n, isexport = %d)",
                    n->decl.did, n->decl.isconst, n->decl.isgeneric, n->decl.isextern, n->decl.isexport);
            outsym(n, fd, depth + 1);
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
            fprintf(fd, "\n");
            outnode(n->loopstmt.init, fd, depth+1);
            outnode(n->loopstmt.cond, fd, depth+1);
            outnode(n->loopstmt.step, fd, depth+1);
            outnode(n->loopstmt.body, fd, depth+1);
            break;
        case Nmatchstmt:
            fprintf(fd, "\n");
            outnode(n->matchstmt.val, fd, depth+1);
            for (i = 0; i < n->matchstmt.nmatches; i++)
                outnode(n->matchstmt.matches[i], fd, depth+1);
            break;
        case Nmatch:
            fprintf(fd, "\n");
            outnode(n->match.pat, fd, depth+1);
            outnode(n->match.block, fd, depth+1);
            break;
        case Nuse:
            fprintf(fd, " (name = %s, islocal = %d)\n", n->use.name, n->use.islocal);
            break;
        case Nexpr:
            ty = tystr(n->expr.type);
            fprintf(fd, " (type = %s, op = %s, flags = %d, did=%zd)\n",
                    ty, opstr(n->expr.op), n->expr.isconst, n->expr.did);
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
                case Lstr:      fprintf(fd, " Lstr %s\n", n->lit.strval); break;
                case Lfunc:
                    fprintf(fd, " Lfunc\n");
                    outnode(n->lit.fnval, fd, depth+1);
                    break;
                case Lseq:
                    fprintf(fd, " Lseq\n");
                    for (i = 0; i < n->lit.nelt; i++)
                        outnode(n->lit.seqval[i], fd, depth+1);
                    break;
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
            if (n->name.ns)
                fprintf(fd, "%s.", n->name.ns);
            fprintf(fd, "%s", n->name.name);
            fprintf(fd, ")\n");
            break;
        case Nnone:
            fprintf(stderr, "Nnone not a real node type!");
            fprintf(fd, "Nnone\n");
            break;
    }
}

void dump(Node *n, FILE *fd)
{
    outnode(n, fd, 0);
}
