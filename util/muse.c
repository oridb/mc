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
int debug;
char debugopt[128];
char **incpaths;
size_t nincpaths;

static void usage(char *prog)
{
    printf("%s [-h] [-o outfile] inputs\n", prog);
    printf("\t-h\tPrint this help\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps\n");
    printf("\t-o\tOutput to outfile\n");
}


int main(int argc, char **argv)
{
    int opt;
    int i;
    Stab *globls;
    Node *rdback;
    FILE *tmp;
    FILE *f;

    while ((opt = getopt(argc, argv, "d::ho:I:")) != -1) {
        switch (opt) {
            case 'o':
                outfile = optarg;
                break;
            case 'h':
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
        globls = mkstab();
        tyinit(globls);
        tokinit(argv[i]);
        file = mkfile(argv[i]);
        file->file.exports = mkstab();
        file->file.globls = globls;
        yyparse();

        infer(file);
	/* before we do anything to the parse */
        if (debugopt['p']) {
            /* test storing tree to file */
            tmp = fopen("a.pkl", "w");
            pickle(file, tmp);
            fclose(tmp);

            /* and reading it back */
            tmp = fopen("a.pkl", "r");
            rdback = unpickle(tmp);
            dump(rdback, stdout);
            fclose(tmp);
	    dump(file, stdout);
        }
	if (!outfile)
	    die("need output file name right now. FIX THIS.");
	f = fopen(outfile, "w");
	writeuse(file, f);
	fclose(f);
    }

    return 0;
}
