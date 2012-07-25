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
    printf("\t-m\ttreat the inputs as usefiles and merge them\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps\n");
    printf("\t-o\tOutput to outfile\n");
    printf("\t-s\tShow the contents of usefiles `inputs`\n");
}

static void dumpuse(char *path)
{
    Stab *globls;
    FILE *f;

    globls = file->file.globls;
    readuse(file, globls);
    f = fopen(path, "r");
    dumpstab(globls, stdout);
    fclose(f);
}

static void genuse(char *path)
{
    Stab *globls;
    FILE *f;

    globls = file->file.globls;
    tyinit(globls);
    tokinit(path);
    yyparse();

    infer(file);
    if (!outfile)
        die("need output file name right now. FIX THIS.");
    f = fopen(outfile, "w");
    writeuse(file, f);
    fclose(f);
}

static void mergeuse(char *path)
{
    Stab *globls;

    globls = file->file.globls;
    readuse(file, globls);
}

int main(int argc, char **argv)
{
    FILE *f;
    int opt;
    int i;

    while ((opt = getopt(argc, argv, "d::ho:I:")) != -1) {
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

    for (i = optind; i < argc; i++) {
        file = mkfile(argv[i]);
        file->file.exports = mkstab();
        file->file.globls = mkstab();
        if (merge)
            mergeuse(argv[i]);
        else if (debugopt['s'])
            dumpuse(argv[i]);
        else
            genuse(argv[i]);
    }
    if (merge) {
        f = fopen(outfile, "w");
        writeuse(file, f);
        fclose(f);
    }

    return 0;
}
