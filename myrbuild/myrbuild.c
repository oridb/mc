#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>
#include <err.h>

#include "parse.h"

#include "../config.h"

/* make libparse happy */
Node *file;
char *filename;

/* options to pass along to the compiler */
int genasm = 0;

/* binaries we call out to */
char *mc = "6m";
char *as = "as";
char *ar = "ar";
char *ld = "ld";
char *muse = "muse";
char *runtime = "_myrrt.o";
char *runtimepath;
/* the name of the output file */
char *libname;
char *binname;
/* additional paths to search for packages */
char **incpaths;
size_t nincpaths;
/* libraries to link against, and their deps */
Htab *libgraph;  /* string -> null terminated string list */
/* the linker script to use */
char *ldscript;

char *sysname;

regex_t usepat;
Htab *compiled; /* used as string set */
Htab *loopdetect; /* used as string set */

static void usage(char *prog)
{
    printf("%s [-h] [-I path] [-l lib] [-b bin] inputs...\n", prog);
    printf("\t-h\tprint this help\n");
    printf("\t-b bin\tBuild a binary called 'bin'\n");
    printf("\t-l lib\tBuild a library called 'name'\n");
    printf("\t-s script\tUse the linker script 'script' when linking\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-S\tGenerate assembly files for all compiled code\n");
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

char *usetomyr(char *path)
{
    char buf[1024];
    /* skip initial quote */
    path++;
    if (!hassuffix(path, ".use\"")) {
        fprintf(stderr, "\"%s, should end with \".use\"\n", path);
        exit(1);
    }
    swapsuffix(buf, 1024, path, ".use\"", ".myr");
    return strdup(buf);
}

void printl(char **lst)
{
    printf("\t");
    printf("%s\t", *lst++);
    while (*lst)
        printf("%s ", *lst++);
    printf("\n");
}

void gencmd(char ***cmd, size_t *ncmd, char *bin, char *file, char **extra, size_t nextra)
{
    size_t i;

    *cmd = NULL;
    *ncmd = 0;
    lappend(cmd, ncmd, bin);
    for (i = 0; i < nincpaths; i++) {
        lappend(cmd, ncmd, "-I");
        lappend(cmd, ncmd, incpaths[i]);
    }
    for (i = 0; i < nextra; i++)
        lappend(cmd, ncmd, extra[i]);
    lappend(cmd, ncmd, file);
    lappend(cmd, ncmd, NULL); 
}

void run(char **cmd)
{
    pid_t pid;
    int status;

    printl(cmd);
    pid = fork();
    status = 0;
    if (pid == -1) {
        err(1, "Could not fork");
    } else if (pid == 0) {
        if (execvp(cmd[0], cmd) == -1)
            err(1, "Failed to exec %s", cmd[0]);
    } else {
        waitpid(pid, &status, 0);
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        exit(WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        die("%s: exited with signal %d\n", cmd[0], WTERMSIG(status));
}

int isfresh(char *from, char *to)
{
    struct stat from_sb, to_sb;

    if (stat(from, &from_sb))
        err(1, "Could not find %s", from);
    if (stat(to, &to_sb) == -1)
        return 0;

    return from_sb.st_mtime <= to_sb.st_mtime;
}

int inlist(char **list, size_t sz, char *str)
{
    size_t i;

    for (i = 0; i < sz; i++)
        if (!strcmp(list[i], str))
            return 1;
    return 0;
}

void getdeps(char *file, char **deps, size_t depsz, size_t *ndeps)
{
    char buf[2048];

    regmatch_t m[2];
    size_t i;
    FILE *f;
    char *dep;

    f = fopen(file, "r");
    if (!f)
        err(1, "Could not open file \"%s\"", file);

    i = 0;
    while (fgets(buf, sizeof buf, f)) {
        if (regexec(&usepat, buf, 2, m, 0) == REG_NOMATCH)
            continue;
        if (i == depsz)
            die("Too many deps for file %s", file);
        dep = strdupn(&buf[m[1].rm_so], m[1].rm_eo - m[1].rm_so);
        if (!inlist(deps, i, dep))
            deps[i++] = dep;
        else
            free(dep);
    }
    fclose(f);
    *ndeps = i;
}

FILE *openlib(char *lib)
{
    FILE *f;
    char buf[1024];
    size_t i;

    f = fopen(buf, "r");
    if (f)
        return f;
    for (i = 0; i < nincpaths; i++) {
        snprintf(buf, sizeof buf, "%s/%s", incpaths[i], lib);
        f = fopen(buf, "r");
        if (f)
            return f;
    }
    err(1, "could not open library file %s\n", lib);
}

void scrapelib(Htab *g, char *lib)
{
    char **deps;
    size_t ndeps;
    FILE *use;
    char *l;

    if (hthas(libgraph, lib))
        return;
    deps = NULL;
    ndeps = 0;
    use = openlib(lib);
    if (fgetc(use) != 'U')
        err(1, "library \"%s\" is not a usefile.", lib);
    /* we don't care about the usefile's name */
    free(rdstr(use));
    while (fgetc(use) == 'L') {
        l = rdstr(use);
        lappend(&deps, &ndeps, l);
        scrapelib(g, l);
    }
    lappend(&deps, &ndeps, NULL);
    htput(g, lib, deps);
}

void compile(char *file)
{
    size_t i, ndeps;
    char **cmd;
    size_t ncmd;
    char *s;
    char *localdep;
    char *deps[512];
    char use[1024];
    char obj[1024];
    char *extra[32];
    size_t nextra = 0;

    if (hthas(compiled, file))
        return;
    if (hthas(loopdetect, file))
        die("Cycle in dependency graph, involving %s\n", file);
    htput(loopdetect, file, file);
    if (hassuffix(file, ".myr")) {
        swapsuffix(use, sizeof use, file, ".myr", ".use");
        swapsuffix(obj, sizeof obj, file, ".myr", ".o");
        getdeps(file, deps, 512, &ndeps);
        for (i = 0; i < ndeps; i++) {
            if (isquoted(deps[i])) {
                localdep = usetomyr(deps[i]);
                compile(localdep);
                free(localdep);
            } else {
                scrapelib(libgraph, deps[i]);
            }
        }
        if (isfresh(file, use))
            goto done;
        if (isfresh(file, obj))
            goto done;
        if (genasm)
            extra[nextra++] = "-S";
        gencmd(&cmd, &ncmd, mc, file, extra, nextra);
        run(cmd);
    } else if (hassuffix(file, ".s")) {
        swapsuffix(obj, sizeof obj, file, ".s", ".o");
        if (isfresh(file, obj))
            goto done;
        extra[nextra++] = "-g";
        extra[nextra++] = "-o";
        extra[nextra++] = obj;
        gencmd(&cmd, &ncmd, as, file, extra, nextra);
        run(cmd);
    }
done:
    s = strdup(file);
    htput(compiled, s, s);
    htdel(loopdetect, file);
}

void mergeuse(char **files, size_t nfiles)
{
    char **args;
    size_t i, nargs;
    char buf[1024];

    args = NULL;
    nargs = 0;
    lappend(&args, &nargs, strdup(muse));
    lappend(&args, &nargs, strdup("-mo"));
    lappend(&args, &nargs, strdup(libname));
    for (i = 0; i < nfiles; i++) {
        if (hassuffix(files[i], ".myr")) {
            swapsuffix(buf, sizeof buf, files[i], ".myr", ".use");
            lappend(&args, &nargs, strdup(buf));
        } else if (!hassuffix(files[i], ".s")) {
            die("Unknown file type %s", files[i]);
        }
    }
    lappend(&args, &nargs, NULL);

    run(args);

    for (i = 0; i < nargs; i++)
        free(args[i]);
    lfree(&args, &nargs);
}

void archive(char **files, size_t nfiles)
{
    char **args;
    size_t i, nargs;
    char buf[1024];

    args = NULL;
    nargs = 0;
    snprintf(buf, sizeof buf, "lib%s.a", libname);
    lappend(&args, &nargs, strdup(ar));
    lappend(&args, &nargs, strdup("-rcs"));
    lappend(&args, &nargs, strdup(buf));
    for (i = 0; i < nfiles; i++) {
        if (hassuffix(files[i], ".myr"))
            swapsuffix(buf, sizeof buf, files[i], ".myr", ".o");
        else if (hassuffix(files[i], ".s"))
            swapsuffix(buf, sizeof buf, files[i], ".s", ".o");
        else
            die("Unknown file type %s", files[i]);
        lappend(&args, &nargs, strdup(buf));
    }
    lappend(&args, &nargs, NULL);

    run(args);

    for (i = 0; i < nargs; i++)
        free(args[i]);
    lfree(&args, &nargs);
}

void visit(char ***args, size_t *nargs, size_t head, Htab *g, char *n, Htab *looped, Htab *marked)
{
    char **deps;
    char buf[1024];

    if (hthas(looped, n))
        err(1, "cycle in library dependency graph involving %s\n", n);
    if (hthas(marked, n))
        return;
    htput(looped, n, n);
    for (deps = htget(g, n); *deps; deps++)
        visit(args, nargs, head, g, *deps, looped, marked);
    htdel(looped, n);
    htput(marked, n, n);
    snprintf(buf, sizeof buf, "-l%s", n);
    linsert(args, nargs, head, strdup(buf));
}

/* topologically sorts the dependency graph of the libraries. */
void addlibs(char ***args, size_t *nargs, Htab *g)
{
    void **libs;
    size_t nlibs;
    size_t i;
    size_t head;
    Htab *looped;
    Htab *marked;

    libs = htkeys(g, &nlibs);
    looped = mkht(strhash, streq);
    marked = mkht(strhash, streq);
    head = *nargs;
    for (i = 0; i < nlibs; i++)
        visit(args, nargs, head, g, libs[i], looped, marked);
}

void linkobj(char **files, size_t nfiles)
{
    char **args;
    size_t i, nargs;
    char buf[1024];

    if (!binname)
        binname = "a.out";

    args = NULL;
    nargs = 0;

    /* ld -T ldscript -o outfile */
    lappend(&args, &nargs, strdup(ld));
    lappend(&args, &nargs, strdup("-o"));
    lappend(&args, &nargs, strdup(binname));

    /* ld -T ldscript */
    if (ldscript) {
        snprintf(buf, sizeof buf, "-T%s", ldscript);
        lappend(&args, &nargs, strdup(buf));
    }

    if (runtimepath) {
        lappend(&args, &nargs, strdup(runtimepath));
    }

    /* ld -T ldscript -o outfile foo.o bar.o baz.o */
    for (i = 0; i < nfiles; i++) {
        if (hassuffix(files[i], ".myr"))
            swapsuffix(buf, sizeof buf, files[i], ".myr", ".o");
        else if (hassuffix(files[i], ".s"))
            swapsuffix(buf, sizeof buf, files[i], ".s", ".o");
        else
            die("Unknown file type %s", files[i]);
        lappend(&args, &nargs, strdup(buf));
    }

    /* ld -T ldscript -o outfile foo.o bar.o baz.o -L/path1 -L/path2 */
    for (i = 0; i < nincpaths; i++) {
        snprintf(buf, sizeof buf, "-L%s", incpaths[i]);
        lappend(&args, &nargs, strdup(buf));
    }

    /* ld -T ldscript -o outfile foo.o bar.o baz.o -L/path1 -L/path2 -llib1 -llib2*/
    addlibs(&args, &nargs, libgraph);

    /* OSX wants a minimum version specified to prevent warnings*/
    if (!strcmp(sysname, "Darwin")) {
        lappend(&args, &nargs, strdup("-macosx_version_min"));
        lappend(&args, &nargs, strdup("10.6"));
    }

    /* the null terminator for exec() */
    lappend(&args, &nargs, NULL);

    run(args);

    for (i = 0; i < nargs; i++)
        free(args[i]);
    lfree(&args, &nargs);
}

void findruntime()
{
    char buf[2048];
    size_t i;

    if (!strcmp(runtime, "none"))
        runtimepath = NULL;
    for (i = 0; i < nincpaths; i++) {
        snprintf(buf, sizeof buf, "%s/%s", incpaths[i], runtime);
        if (access(buf, R_OK) == 0) {
            printf("Got %s\n", buf);
            runtimepath = strdup(buf);
            return;
        }
    }
    err(1, "Could not find runtime %s", runtime);
}

int main(int argc, char **argv)
{
    int opt;
    int i;
    struct utsname name;

    if (uname(&name) == 0)
        sysname = strdup(name.sysname);
    lappend(&incpaths, &nincpaths, strdup(Instroot "/lib/myr"));
    while ((opt = getopt(argc, argv, "hb:l:s:r:SI:C:A:M:L:R:")) != -1) {
        switch (opt) {
            case 'b': binname = optarg; break;
            case 'l': libname = optarg; break;
            case 's': ldscript = optarg; break;
            case 'S': genasm = 1; break;
            case 'C': mc = optarg; break;
            case 'A': as = optarg; break;
            case 'M': muse = optarg; break;
            case 'L': ld = optarg; break;
            case 'R': ar = optarg; break;
            case 'r': runtime = optarg; break;
            case 'I':
                lappend(&incpaths, &nincpaths, strdup(optarg));
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

    libgraph = mkht(strhash, streq);
    compiled = mkht(strhash, streq);
    loopdetect = mkht(strhash, streq);
    findruntime();
    regcomp(&usepat, "^[[:space:]]*use[[:space:]]+([^[:space:]]+)", REG_EXTENDED);
    for (i = optind; i < argc; i++)
        compile(argv[i]);
    if (libname) {
        mergeuse(&argv[optind], argc - optind);
        archive(&argv[optind], argc - optind);
    } else {
        linkobj(&argv[optind], argc - optind);
    }

    return 0;
}
