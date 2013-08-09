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
char *outfile;
int fromuse;
int debug;
char debugopt[128];
char **incpaths;
size_t nincpaths;

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
            case 'u':
                fromuse = 1;
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
        lappend(&incpaths, &nincpaths, Instroot "/lib/myr");
        file = mkfile(argv[i]);
        file->file.exports = mkstab();
        file->file.globls = mkstab();
        if (fromuse) {
            f = fopen(argv[i], "r");
            if (!f)
                die("Unable to open usefile %s\n", argv[i]);
            loaduse(f, file->file.globls);
        } else {
            tyinit(file->file.globls);
            tokinit(argv[i]);
            yyparse();
            infer(file);
        }
        dumpstab(file->file.globls, stdout);
    }

    return 0;
}
