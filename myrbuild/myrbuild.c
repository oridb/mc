#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>

#include "parse.h"

#include "../config.h"

/* make libparse happy */
Node *file;
char *filename;

/* the name of the output file */
char *libname;
char *binname;
/* additional paths to search for packages */
char **incpaths;
size_t nincpaths;
/* libraries to link against. */
char **libs;
size_t nlibs;

regex_t usepat;

static void usage(char *prog)
{
    printf("%s [-h] [-I path] [-l lib] [-b bin] inputs...\n", prog);
    printf("\t-h\tprint this help\n");
    printf("\t-b bin\tBuild a binary called 'bin'\n");
    printf("\t-l lib\tBuild a library called 'name'\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
}

int hassuffix(char *path, char *suffix)
{
    int pathlen;
    int sufflen;

    pathlen = strlen(path);
    sufflen = strlen(suffix);
    
    if (sufflen > pathlen)
        return 0;
    return !strcmp(&path[pathlen-sufflen], suffix);
}

int isquoted(char *path)
{
    return path[0] == '"' && path[strlen(path) - 1] == '"';
}

char *fromuse(char *path)
{
    char buf[1024];
    /* skip initial quote */
    path++;
    swapsuffix(buf, 1024, path, ".use\"", ".myr");
    return strdup(buf);
}

int run(char *bin, char **inc, size_t ninc, char *file)
{
    char **args;
    size_t i, nargs;
    pid_t pid;
    int status;

    nargs = 0;
    args = NULL;
    
    lappend(&args, &nargs, bin);
    for (i = 0; i < nincpaths; i++) {
        lappend(&args, &nargs, "-I");
        lappend(&args, &nargs, incpaths[i]);
    }
    lappend(&args, &nargs, file);
    lappend(&args, &nargs, NULL); 
    for (i = 0; args[i]; i++)
        printf("%s\t", args[i]);
    printf("\n");
    pid = fork();
    if (pid == -1) {
        die("Could not fork\n");
    } else if (pid == 0) {
        if (execvp(bin, args) == -1)
            die("Failed to exec %s\n", bin);
    } else {
        waitpid(pid, &status, 0);
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int isfresh(char *from, char *to)
{
    struct stat from_sb, to_sb;

    if (stat(from, &from_sb))
        die("Could not find %s\n", from);
    if (stat(to, &to_sb) == -1)
        return 1;

    return from_sb.st_mtime >= to_sb.st_mtime;
}

void getdeps(char *file, char **deps, size_t depsz, size_t *ndeps)
{
    char buf[2048]; /* if you hit this limit, shoot yourself */

    regmatch_t m[2];
    size_t i;
    FILE *f;

    f = fopen(file, "r");
    if (!f)
        die("Could not open file %s\n", file);

    i = 0;
    while (fgets(buf, sizeof buf, f)) {
        if (regexec(&usepat, buf, 2, m, 0) == REG_NOMATCH)
            continue;
        if (i == depsz)
            die("Too many deps for file %s\n", file);
        deps[i++] = strdupn(&buf[m[1].rm_so], m[1].rm_eo - m[1].rm_so);
    }
    *ndeps = i;
}

void compile(char *file)
{
    size_t i, ndeps;
    char *localdep;
    char *deps[512];
    char buf[1024];

    if (hassuffix(file, ".myr")) {
        getdeps(file, deps, 512, &ndeps);
        for (i = 0; i < ndeps; i++) {
            if (isquoted(deps[i])) {
                localdep = fromuse(deps[i]);
                compile(localdep);
                free(localdep);
            } else {
                lappend(&libs, &nlibs, deps[i]);
            }
        }
        swapsuffix(buf, sizeof buf, file, ".myr", ".use");
        if (isfresh(file, buf))
            run("muse", incpaths, nincpaths, file);

        swapsuffix(buf, sizeof buf, file, ".myr", ".o");
        if (isfresh(file, buf))
            run("6m", incpaths, nincpaths, file);
    } else if (hassuffix(file, ".s")) {
    }
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

    regcomp(&usepat, "^[[:space:]]*use[[:space:]]+([^[:space:]]+)", REG_EXTENDED);
    for (i = optind; i < argc; i++)
        compile(argv[i]);

    return 0;
}
