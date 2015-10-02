#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
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
char *outfile;
int show;
char debugopt[128];
char **incpaths;
size_t nincpaths;
char **extralibs;
size_t nextralibs;

static void usage(char *prog)
{
    printf("%s [-hIdos] [-o outfile] [-m] inputs\n", prog);
    printf("\t-h\tprint this help\n");
    printf("\t\tThe outfile must be the same name as each package merged.\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps\n");
    printf("\t-o out\tOutput to outfile\n");
    printf("\t-s\tShow the contents of usefiles `inputs`\n");
}

static void mergeuse(char *path)
{
    FILE *f;
    Stab *st;

    st = file->file.globls;
    f = fopen(path, "r");
    if (!f)
        die("Couldn't open %s\n", path);
    loaduse(path, f, st, Visexport);
    fclose(f);
}

int main(int argc, char **argv)
{
    Optctx ctx;
    size_t i;
    FILE *f;

    optinit(&ctx, "d:hmo:I:l:", argv, argc);
    while (!optdone(&ctx)) {
        switch (optnext(&ctx)) {
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'o':
                outfile = ctx.optarg;
                break;
            case 'd':
                while (ctx.optarg && *ctx.optarg)
                    debugopt[*ctx.optarg++ & 0x7f] = 1;
                break;
            case 'I':
                lappend(&incpaths, &nincpaths, ctx.optarg);
                break;
            case 'l':
                lappend(&extralibs, &nextralibs, ctx.optarg);
                break;
            case 's':
                show = 1;
                break;
            default:
                usage(argv[0]);
                exit(0);
                break;
        }
    }

    lappend(&incpaths, &nincpaths, Instroot "/lib/myr");
    if (!outfile) {
        fprintf(stderr, "output file needed when merging usefiles.\n");
        exit(1);
    }

    /* read and parse the file */
    file = mkfile("internal");
    file->file.globls = mkstab(0);
    updatens(file->file.globls, outfile);
    tyinit(file->file.globls);
    for (i = 0; i < ctx.nargs; i++)
        mergeuse(ctx.args[i]);
    infer(file);
    tagexports(file, 1);
    addextlibs(file, extralibs, nextralibs);

    /* generate the usefile */
    f = fopen(outfile, "w");
    if (debugopt['s'] || show)
        dumpstab(file->file.globls, stdout);
    else
        writeuse(f, file);
    fclose(f);
    return 0;
}
