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
#include "opt.h"
#include "asm.h"

#include "../config.h"

/* FIXME: move into one place...? */
Node *file;
int debug;
char debugopt[128];
char *outfile;
char **incpaths;
size_t nincpaths;

static void usage(char *prog)
{
    printf("%s [-h] [-o outfile] [-d[dbgopts]] inputs\n", prog);
    printf("\t-h\tPrint this help\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps. Recognized options: f r p i\n");
    printf("\t\t\tno options: print most common debug information\n");
    printf("\t\t\tf: additionally log folded trees\n");
    printf("\t\t\tl: additionally log lowered pre-cfg trees\n");
    printf("\t\t\tT: additionally log tree immediately\n");
    printf("\t\t\tr: additionally log register allocation activity\n");
    printf("\t\t\ti: additionally log instruction selection activity\n");
    printf("\t-o\tOutput to outfile\n");
    printf("\t-S\tGenerate assembly instead of object code\n");
}

static void assem(char *f)
{
    char objfile[1024];
    char cmd[1024];

    swapsuffix(objfile, 1024, f, ".s", ".o");
    snprintf(cmd, 1024, Asmcmd, objfile, f);
    if (system(cmd) == -1)
        die("Couldn't run assembler");
}

int main(int argc, char **argv)
{
    int opt;
    int i;
    Stab *globls;
    char buf[1024];

    lappend(&incpaths, &nincpaths, Instroot "include/myr");
    while ((opt = getopt(argc, argv, "d::hSo:I:")) != -1) {
        switch (opt) {
            case 'o':
                outfile = optarg;
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'd':
                debug = 1;
                while (optarg && *optarg) {
                    if (*optarg == 'y')
                        yydebug = 1;
                    debugopt[*optarg++ & 0x7f]++;
                }
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

        /* before we do anything to the parse */
        if (debugopt['T'])
            dump(file, stdout);
        infer(file);
        /* after all processing */
        if (debug)
            dump(file, stdout);

        swapsuffix(buf, 1024, argv[i], ".myr", ".s");
        gen(file, buf);
        assem(buf);
    }

    return 0;
}
