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

/* FIXME: move into one place...? */
Node *file;
int debug;
int asmonly;
char *outfile;
char **incpaths;
size_t nincpaths;

static void usage(char *prog)
{
    printf("%s [-h] [-o outfile] inputs\n", prog);
    printf("\t-h\tPrint this help\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps\n");
    printf("\t-o\tOutput to outfile\n");
    printf("\t-S\tGenerate assembly instead of object code\n");
}

char *outfmt(char *buf, size_t sz, char *infile, char *outfile)
{
    char *p, *suffix;
    size_t len;

    if (outfile) {
        snprintf(buf, sz, "%s", outfile);
        return buf;
    }

    if (asmonly)
        suffix = ".s";
    else
        suffix = ".o";

    p = strrchr(infile, '.');
    if (p)
        len = (p - infile);
    else
        len = strlen(infile);
    if (len + strlen(suffix) >= sz)
        die("Output file name too long");
    strncpy(buf, infile, len);
    strcat(buf, suffix);

    return buf;
}

int main(int argc, char **argv)
{
    int opt;
    int i;
    Stab *globls;
    char buf[1024];

    while ((opt = getopt(argc, argv, "dhSo:I:")) != -1) {
        switch (opt) {
            case 'o':
                outfile = optarg;
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'd':
                debug++;
                break;
            case 'S':
                asmonly++;
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
        if (debug)
            dump(file, stdout);
        infer(file);
        /* after all processing */
        if (debug)
            dump(file, stdout);

        gen(file, outfmt(buf, 1024, argv[i], outfile));
    }

    return 0;
}
