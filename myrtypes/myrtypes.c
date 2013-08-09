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

#include "../config.h"

/* FIXME: move into one place...? */
Node *file;
int fromuse;
int debug;
char debugopt[128];
char **incpaths;
size_t nincpaths;

static void printindent(int n)
{
    int i;
    for (i = 0; i < n; i++)
        printf("  ");
}

static void dumptypes(Node *n, int indent)
{
    size_t i;
    char *ty;

    if (!n)
        return;
    switch (n->type) {
        case Nfile:
            for (i = 0; i < n->file.nuses; i++)
                dumptypes(n->file.uses[i], indent);
            for (i = 0; i < n->file.nstmts; i++)
                dumptypes(n->file.stmts[i], indent);
            break;
        case Ndecl:
            printindent(indent);
            if (n->decl.isconst)
                printf("const ");
            else
                printf("var ");
            ty = tystr(n->decl.type);
            printf("%s : %s\n", namestr(n->decl.name), ty);
            free(ty);
            dumptypes(n->decl.init, indent + 1);
            break;
        case Nblock:
            for (i = 0; i < n->block.nstmts; i++)
                dumptypes(n->block.stmts[i], indent);
            break;
        case Nifstmt:
            dumptypes(n->ifstmt.cond, indent);
            dumptypes(n->ifstmt.iftrue, indent);
            dumptypes(n->ifstmt.iffalse, indent);
            break;
        case Nloopstmt:
            dumptypes(n->loopstmt.init, indent);
            dumptypes(n->loopstmt.cond, indent);
            dumptypes(n->loopstmt.step, indent);
            dumptypes(n->loopstmt.body, indent);
            break;
        case Nmatchstmt:
            dumptypes(n->matchstmt.val, indent);
            for (i = 0; i < n->matchstmt.nmatches; i++)
                dumptypes(n->matchstmt.matches[i], indent);
            break;
        case Nmatch:
            dumptypes(n->match.pat, indent);
            dumptypes(n->match.block, indent);
            break;
        case Nuse:
            printindent(indent);
            if (n->use.islocal)
                printf("Use \"%s\"\n", n->use.name);
            else
                printf("Use %s\n", n->use.name);
            break;
        case Nexpr:
            dumptypes(n->expr.idx, indent);
            for (i = 0; i < n->expr.nargs; i++)
                dumptypes(n->expr.args[i], indent);
            break;
        case Nlit:
            switch (n->lit.littype) {
                case Lfunc: dumptypes(n->lit.fnval, indent); break;
                default: break;
            }
            break;
        case Nfunc:
            printindent(indent);
            printf("Args:\n");
            for (i = 0; i < n->func.nargs; i++)
                dumptypes(n->func.args[i], indent+1);
            printindent(indent);
            printf("Body:\n");
            dumptypes(n->func.body, indent + 1);
            break;
        case Nname:
            break;
        case Nnone:
            die("Nnone not a real node type!");
            break;
    }
}

void dumpusetypes(Stab *st, int indent)
{
    size_t i, n;
    void **k;

    /* decls */
    k = htkeys(st->dcl, &n);
    for (i = 0; i < n; i++) {
        dumptypes(getdcl(st, k[i]), indent);
    }
    free(k);

    /* sub-namespaces */
    k = htkeys(st->ns, &n);
    for (i = 0; i < n; i++) {
        printindent(indent + 1);
        printf("namespace %s:\n", (char*)k[i]);
        dumpusetypes(getns_str(st, k[i]), indent + 1);
    }
    free(k);
}

static void usage(char *prog)
{
    printf("%s [-hu] [-d opt][-I path] inputs\n", prog);
    printf("\t-h\tprint this help\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps\n");
    printf("\t-u\tLoad the symbols to dump from a use file\n");
}

int main(int argc, char **argv)
{
    FILE *f;
    int opt;
    int i;

    while ((opt = getopt(argc, argv, "hud:I:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'u':
                fromuse = 1;
                break;
            case 'd':
                debug = 1;
                while (optarg && *optarg)
                    debugopt[*optarg++ & 0x7f] = 1;
                break;
            case 'I':
                lappend(&incpaths, &nincpaths, optarg);
                break;
            default:
                usage(argv[0]);
                exit(0);
                break;
        }
    }

    for (i = optind; i < argc; i++) {
        lappend(&incpaths, &nincpaths, Instroot "/lib/myr");
        file = mkfile(argv[i]);
        file->file.exports = mkstab();
        file->file.globls = mkstab();
        printf("%s:\n", argv[i]);
        if (fromuse) {
            f = fopen(argv[i], "r");
            if (!f)
                die("Unable to open usefile %s\n", argv[i]);
            loaduse(f, file->file.globls);
            dumpusetypes(file->file.globls, 0);
        } else {
            tyinit(file->file.globls);
            tokinit(argv[i]);
            yyparse();
            infer(file);
            dumptypes(file, 1);
        }
    }

    return 0;
}
