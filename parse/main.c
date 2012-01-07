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

Node *file;
static char *outfile;
int debug;

static void usage(char *prog)
{
    printf("%s [-h] [-o outfile] inputs\n", prog);
    printf("\t-h\tPrint this help\n");
    printf("\t-o\tOutput to outfile\n");
}

int main(int argc, char **argv)
{
    int opt;
    int i;
    Node *rdback;
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
        tyinit();
        tokinit(argv[i]);
        file = mkfile(argv[i]);
        file->file.exports = mkstab();
        file->file.globls = mkstab();
        yyparse();

        /* before we do anything to the parse */
        dump(file, stdout);

        infer(file);

        /* test storing tree to file */
        tmp = fopen("test.pkl", "w");
        pickle(file, tmp);
        fclose(tmp);

        /* and reading it back */
        tmp = fopen("test.pkl", "r");
        rdback = unpickle(tmp);
        dump(rdback, stdout);
        fclose(tmp);

        /* after all processing */
        dump(file, stdout);
        gen();
    }

    return 0;
}

void gen(void)
{
    printf("GEN!\n");
}
