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
#include <sys/types.h>
#include <regex.h>

#include "parse.h"

/* make libparse happy */
Node *file;

char *libname;
char *binname;
char **incpaths;
size_t nincpaths;

regex_t usepat;

static void usage(char *prog)
{
    printf("%s [-h] [-I path] [-l lib] [-b bin] inputs...\n", prog);
    printf("\t-h\tprint this help\n");
    printf("\t-b bin\tBuild a binary called 'bin'\n");
    printf("\t-l lib\tBuild a library called 'name'\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
}

void getdeps(char *file)
{
    char buf[2048]; /* if you hit this limit, shoot yourself */

    regmatch_t m[2];
    char *usepath;
    FILE *f;

    f = fopen(file, "r");
    if (!f)
        die("Could not open file %s\n", file);

    while (fgets(buf, sizeof buf, f)) {
        if (regexec(&usepat, buf, 2, m, 0) == REG_NOMATCH)
            continue;
        usepath = strdupn(&buf[m[1].rm_so], m[1].rm_eo - m[1].rm_so);
        printf("use = %s\n", usepath);
    }
}

void compile()
{
}

int main(int argc, char **argv)
{
    int opt;
    int i;

    while ((opt = getopt(argc, argv, "hb:l:I:")) != -1) {
        switch (opt) {
            case 'b':
                binname = optarg;
                break;
            case 'l':
                libname = optarg;
                break;
            case 'I':
                lappend(&incpaths, &nincpaths, optarg);
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            default:
                usage(argv[0]);
                exit(0);
                break;
        }
    }

    if (libname && binname)
        die("Can't specify both library and binary names");

    switch (regcomp(&usepat, "^[[:space:]]*use[[:space:]]+([^[:space:]]+)", REG_EXTENDED)) {
        case REG_BADBR: die("case REG_BADBR:"); break;
        case REG_BADPAT: die("case REG_BADPAT:"); break;
        case REG_BADRPT: die("case REG_BADRPT:"); break;
        case REG_EBRACE: die("case REG_EBRACE:"); break;
        case REG_EBRACK: die("case REG_EBRACK:"); break;
        case REG_ECOLLATE: die("case REG_ECOLLATE:"); break;
        case REG_EEND: die("case REG_EEND:"); break;
        case REG_EESCAPE: die("case REG_EESCAPE:"); break;
        case REG_EPAREN: die("case REG_EPAREN:"); break;
        case REG_ERANGE: die("case REG_ERANGE:"); break;
        case REG_ESIZE: die("case REG_ESIZE:"); break;
        case REG_ESPACE: die("case REG_ESPACE:"); break;
        case REG_ESUBREG: die("case REG_ESUBREG:"); break;
    }
    for (i = optind; i < argc; i++)
        getdeps(argv[i]);

    return 0;
}
