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
    loaduse(f, globls);
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
    tagexports(file->file.exports, 0);
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

    st = file->file.exports;
    f = fopen(path, "r");
    if (!f)
        die("Couldn't open %s\n", path);
    loaduse(f, st);
    fclose(f);
}

int main(int argc, char **argv)
{
    FILE *f;
    int opt;
    int i;

    while ((opt = getopt(argc, argv, "d::hmo:I:")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'm':
                merge = 1;
                break;
            case 'o':
                outfile = optarg;
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

    lappend(&incpaths, &nincpaths, Instroot "/lib/myr");
    if (merge) {
        if (!outfile) {
            fprintf(stderr, "Output file needed when merging usefiles.");
            exit(1);
        }

        file = mkfile("internal");
        file->file.exports = mkstab();
        file->file.globls = mkstab();
        updatens(file->file.exports, outfile);
        tyinit(file->file.globls);
        for (i = optind; i < argc; i++)
            mergeuse(argv[i]);
        infer(file);
        tagexports(file->file.exports, 1);
        f = fopen(outfile, "w");
        writeuse(f, file);
        fclose(f);
    } else {
        for (i = optind; i < argc; i++) {
            file = mkfile(argv[i]);
            file->file.exports = mkstab();
            file->file.globls = mkstab();
            if (debugopt['s'])
                dumpuse(argv[i]);
            else
                genuse(argv[i]);
        }
    }

    return 0;
}
