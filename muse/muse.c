#include <stdlib.h>
#include <stdio.h>
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
int merge;
int debug;
char debugopt[128];
char **incpaths;
size_t nincpaths;

static void usage(char *prog)
{
    printf("%s [-hIdos] [-o outfile] [-m] inputs\n", prog);
    printf("\t-h\tprint this help\n");
    printf("\t-m\ttreat the inputs as usefiles and merge them into outfile\n");
    printf("\t\tThe outfile must be the same name as each package merged.\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps\n");
    printf("\t-o out\tOutput to outfile\n");
    printf("\t-s\tShow the contents of usefiles `inputs`\n");
}

static void dumpuse(char *path)
{
    Stab *globls;
    FILE *f;

    globls = file->file.globls;
    f = fopen(path, "r");
    loaduse(f, globls, Visexport);
    fclose(f);
    dumpstab(globls, stdout);
}

static void genuse(char *path)
{
    Stab *globls;
    char *p;
    FILE *f;
    char buf[1024];

    globls = file->file.globls;
    tyinit(globls);
    tokinit(path);
    yyparse();

    infer(file);
    tagexports(file->file.globls, 0);
    if (outfile) {
        p = outfile;
    } else {
        swapsuffix(buf, sizeof buf, path, ".myr", ".use");
        p = buf;
    }
    f = fopen(p, "w");
    writeuse(f, file);
    fclose(f);
}

static void mergeuse(char *path)
{
    FILE *f;
    Stab *st;

    st = file->file.globls;
    f = fopen(path, "r");
    if (!f)
        die("Couldn't open %s\n", path);
    loaduse(f, st, Visexport);
    fclose(f);
}

int main(int argc, char **argv)
{
    Optctx ctx;
    size_t i;
    FILE *f;

    optinit(&ctx, "d:hmo:I:", argv, argc);
    while (!optdone(&ctx)) {
        switch (optnext(&ctx)) {
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'm':
                merge = 1;
                break;
            case 'o':
                outfile = ctx.optarg;
                break;
            case 'd':
                debug = 1;
                while (ctx.optarg && *ctx.optarg)
                    debugopt[*ctx.optarg++ & 0x7f] = 1;
                break;
            case 'I':
                lappend(&incpaths, &nincpaths, ctx.optarg);
                break;
            default:
                usage(argv[0]);
                exit(0);
                break;
        }
    }

    lappend(&incpaths, &nincpaths, Instroot "/lib/myr");
    if (merge) {
        if (!outfile) {
            fprintf(stderr, "Output file needed when merging usefiles.");
            exit(1);
        }

        file = mkfile("internal");
        file->file.globls = mkstab();
        updatens(file->file.globls, outfile);
        tyinit(file->file.globls);
        for (i = 0; i < ctx.nargs; i++)
            mergeuse(ctx.args[i]);
        infer(file);
        tagexports(file->file.globls, 1);
        f = fopen(outfile, "w");
        if (debugopt['s'])
            dumpstab(file->file.globls, stdout);
        writeuse(f, file);
        fclose(f);
    } else {
        for (i = 0; i < ctx.nargs; i++) {
            file = mkfile(ctx.args[i]);
            file->file.globls = mkstab();
            if (debugopt['s'])
                dumpuse(ctx.args[i]);
            else
                genuse(ctx.args[i]);
        }
    }

    return 0;
}
