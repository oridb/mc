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
#include "asm.h"

Node *file;
static char *outfile;
int debug;

static void usage(char *prog)
{
    printf("%s [-h] [-o outfile] inputs\n", prog);
    printf("\t-h\tPrint this help\n");
    printf("\t-d\tPrint debug dumps\n");
    printf("\t-o\tOutput to outfile\n");
}

int main(int argc, char **argv)
{
    int opt;
    int i;
    Node *rdback;
    Stab *globls;
    FILE *tmp;

    while ((opt = getopt(argc, argv, "dho:")) != -1) {
        switch (opt) {
            case 'o':
                outfile = optarg;
                break;
            case 'h':
            case 'd':
                debug++;
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

        if (debug) {
          /* before we do anything to the parse */
          dump(file, stdout);
        }

        infer(file);

        /* test storing tree to file */
        tmp = fopen("a.pkl", "w");
        pickle(file, tmp);
        fclose(tmp);

        if (debug) {
          /* and reading it back */
          tmp = fopen("test.pkl", "r");
          rdback = unpickle(tmp);
          dump(rdback, stdout);
          fclose(tmp);

          /* after all processing */
          dump(file, stdout);
        }
        gen(file, "a.s");
    }

    return 0;
}
