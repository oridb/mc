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
char debugopt[128];
int writeasm;
char *outfile;
char **incpaths;
size_t nincpaths;

static void usage(char *prog)
{
    printf("%s [-h] [-o outfile] [-d[dbgopts]] inputs\n", prog);
    printf("\t-h\tPrint this help\n");
    printf("\t-S\tWrite out `input.s` when compiling\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps. Recognized options: f r p i\n");
    printf("\t\t\tf: log folded trees\n");
    printf("\t\t\tl: log lowered pre-cfg trees\n");
    printf("\t\t\tT: log tree immediately\n");
    printf("\t\t\tr: log register allocation activity\n");
    printf("\t\t\ti: log instruction selection activity\n");
    printf("\t\t\tu: log type unifications\n");
    printf("\t-o\tOutput to outfile\n");
    printf("\t-S\tGenerate assembly instead of object code\n");
}

static void assem(char *asmsrc, char *input)
{
    char objfile[1024];
    char cmd[2048];

    swapsuffix(objfile, 1024, input, ".myr", ".o");
    snprintf(cmd, 1024, Asmcmd, objfile, asmsrc);
    if (system(cmd) == -1)
        die("Couldn't run assembler");
}

static char *gentemp(char *buf, size_t bufsz, char *path, char *suffix)
{
    char *tmpdir;
    char *base;

    tmpdir = getenv("TMPDIR");
    if (!tmpdir)
        tmpdir = "/tmp";
    base = strrchr(path, '/');
    if (base)
        base++;
    else
        base = path;
    snprintf(buf, bufsz, "%s/tmp%lx-%s%s", tmpdir, random(), base, suffix);
    return buf;
}

int main(int argc, char **argv)
{
    int opt;
    int i;
    Stab *globls;
    char buf[1024];

    while ((opt = getopt(argc, argv, "d:hSo:I:")) != -1) {
        switch (opt) {
            case 'o':
                outfile = optarg;
                break;
            case 'S':
                writeasm = 1;
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'd':
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

    lappend(&incpaths, &nincpaths, Instroot "/lib/myr");
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
        /* after all type inference */
        if (debugopt['t'])
            dump(file, stdout);

        if (writeasm) {
            swapsuffix(buf, sizeof buf, argv[i], ".myr", ".s");
        } else {
            gentemp(buf, sizeof buf, argv[i], ".s");
        }
        gen(file, buf);
        assem(buf, argv[i]);
    }

    return 0;
}
