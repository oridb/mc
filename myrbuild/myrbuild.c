#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"

#include "../config.h"

/* make libparse happy */
Node *file;
char *filename;

/* options to pass along to the compiler */
int genasm = 0;

/* binaries we call out to */
char *mc = "6m";
char *muse = "muse";
char *as[] = Asmcmd;
char *ar[] = Arcmd;
char *ld[] = Ldcmd;
char *runtime = Instroot "/lib/myr/_myrrt" Objsuffix;
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

Htab *compiled; /* used as string set */
Htab *loopdetect; /* used as string set */

static void fail(int status, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(status);
}

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
        fail(1, "Could not fork\n");
    } else if (pid == 0) {
        if (execvp(cmd[0], cmd) == -1)
            fail(1, "Failed to exec %s\n", cmd[0]);
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
        fail(1, "Could not find %s\n", from);
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

int finddep(char *buf, char **dep)
{
    char *end, *w, *p;
    p = buf;


    end = buf + strlen(buf);
    while (isspace(*p) && p != end)
        p++;
    if (strncmp(p, "use", 3) != 0)
        return 0;
    p += 3;
    if (!isspace(*p))
        return 0;
    while (isspace(*p) && *p != ';' && *p != '"' && p != end)
        p++;

    w = p;
    while (!isspace(*p) && p != end)
        p++;
    if (p == w)
        return 0;
    *dep = strdupn(w, p - w);
    return 1;
}

void getdeps(char *file, char **deps, size_t depsz, size_t *ndeps)
{
    char buf[2048];

    size_t i;
    FILE *f;
    char *dep;

    f = fopen(file, "r");
    if (!f)
        fail(1, "Could not open file \"%s\"\n", file);

    i = 0;
    while (fgets(buf, sizeof buf, f)) {
        if (!finddep(buf, &dep))
            continue;
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

    for (i = 0; i < nincpaths; i++) {
        snprintf(buf, sizeof buf, "%s/%s", incpaths[i], lib);
        f = fopen(buf, "r");
        if (f)
            return f;
    }
    snprintf(buf, sizeof buf, "%s/%s", Instroot "/lib/myr", lib);
    f = fopen(buf, "r");
    if (f)
        return f;
    fail(1, "could not open library file %s\n", lib);
    return NULL;
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
        fail(1, "library \"%s\" is not a usefile.\n", lib);
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

void compile(char *file, char ***stack, size_t *nstack)
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
    if (hthas(loopdetect, file)) {
        fprintf(stderr, "Cycle in dependency graph, involving %s. dependency stack:\n", file);
        for (i = 0; i < *nstack; i++)
            fprintf(stderr, "\t%s\n", (*stack)[i]);
        exit(1);
    }
    htput(loopdetect, file, file);
    if (hassuffix(file, ".myr")) {
        swapsuffix(use, sizeof use, file, ".myr", ".use");
        swapsuffix(obj, sizeof obj, file, ".myr", Objsuffix);
        getdeps(file, deps, 512, &ndeps);
        for (i = 0; i < ndeps; i++) {
            if (isquoted(deps[i])) {
                localdep = usetomyr(deps[i]);
                lappend(stack, nstack, localdep);
                compile(localdep, stack, nstack);
                lpop(stack, nstack);
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
        swapsuffix(obj, sizeof obj, file, ".s", Objsuffix);
        if (isfresh(file, obj))
            goto done;
        for (i = 1; as[i]; i++)
            extra[nextra++] = as[i];
        extra[nextra++] = obj;
        gencmd(&cmd, &ncmd, as[0], file, extra, nextra);
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
    lappend(&args, &nargs, strdup("-o"));
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
    for (i = 0; ar[i]; i++)
         lappend(&args, &nargs, strdup(ar[i]));
    lappend(&args, &nargs, strdup(buf));
    for (i = 0; i < nfiles; i++) {
        if (hassuffix(files[i], ".myr"))
            swapsuffix(buf, sizeof buf, files[i], ".myr", Objsuffix);
        else if (hassuffix(files[i], ".s"))
            swapsuffix(buf, sizeof buf, files[i], ".s", Objsuffix);
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

void findlib(char *buf, size_t bufsz, char *lib)
{
    size_t i;

    for (i = 0; i < nincpaths; i++) {
        snprintf(buf, bufsz, "%s/lib%s.a", incpaths[i], lib);
        if (access(buf, F_OK) == 0)
            return;
    }
    snprintf(buf, bufsz, "%s/lib/myr/lib%s.a", Instroot, lib);
    if (access(buf, F_OK) == 0)
        return;
    fail(1, "unable to find library lib%s.a\n", lib);
}

void visit(char ***args, size_t *nargs, size_t head, Htab *g, char *n, Htab *looped, Htab *marked)
{
    char **deps;
    char buf[1024];

    if (hthas(looped, n))
        fail(1, "cycle in library dependency graph involving %s\n", n);
    if (hthas(marked, n))
        return;
    htput(looped, n, n);
    for (deps = htget(g, n); *deps; deps++)
        visit(args, nargs, head, g, *deps, looped, marked);
    htdel(looped, n);
    htput(marked, n, n);
    if (!strcmp(sysname, "Plan9"))
        findlib(buf, sizeof buf, n);
    else
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
    for (i = 0; ld[i]; i++)
        lappend(&args, &nargs, strdup(ld[i]));
    lappend(&args, &nargs, strdup(binname));

    /* ld -T ldscript */
    if (ldscript) {
        snprintf(buf, sizeof buf, "-T%s", ldscript);
        lappend(&args, &nargs, strdup(buf));
    }

    if (runtime) {
        lappend(&args, &nargs, strdup(runtime));
    }

    /* ld -T ldscript -o outfile foo.o bar.o baz.o */
    for (i = 0; i < nfiles; i++) {
        if (hassuffix(files[i], ".myr"))
            swapsuffix(buf, sizeof buf, files[i], ".myr", Objsuffix);
        else if (hassuffix(files[i], ".s"))
            swapsuffix(buf, sizeof buf, files[i], ".s", Objsuffix);
        else
            die("Unknown file type %s", files[i]);
        lappend(&args, &nargs, strdup(buf));
    }

    /* ld -T ldscript -o outfile foo.o bar.o baz.o -L/path1 -L/path2 */
    if (strcmp(sysname, "Plan9") != 0) {
        for (i = 0; i < nincpaths; i++) {
            snprintf(buf, sizeof buf, "-L%s", incpaths[i]);
            lappend(&args, &nargs, strdup(buf));
        }
        snprintf(buf, sizeof buf, "-L%s%s", Instroot, "/lib/myr");
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

int main(int argc, char **argv)
{
    struct utsname name;
    char **stack;
    size_t nstack;
    Optctx ctx;
    size_t i;

    if (uname(&name) == 0)
        sysname = strdup(name.sysname);
    
    optinit(&ctx, "hb:l:s:r:SI:C:A:M:L:R:", argv, argc);
    while (!optdone(&ctx)) {
        switch (optnext(&ctx)) {
            case 'b': binname = ctx.optarg; break;
            case 'l': libname = ctx.optarg; break;
            case 's': ldscript = ctx.optarg; break;
            case 'S': genasm = 1; break;
            case 'C': mc = ctx.optarg; break;
            case 'M': muse = ctx.optarg; break;
            case 'A': as[0] = ctx.optarg; break;
            case 'L': ld[0] = ctx.optarg; break;
            case 'R': ar[0] = ctx.optarg; break;
            case 'r':
                      if (!strcmp(ctx.optarg, "none"))
                          runtime = NULL;
                      else
                          runtime = strdup(ctx.optarg);
                      break;
            case 'I':
                lappend(&incpaths, &nincpaths, strdup(ctx.optarg));
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

    stack = NULL;
    nstack = 0;
    libgraph = mkht(strhash, streq);
    compiled = mkht(strhash, streq);
    loopdetect = mkht(strhash, streq);
    for (i = 0; i < ctx.nargs; i++) {
        lappend(&stack, &nstack, ctx.args[i]);
        compile(ctx.args[i], &stack, &nstack);
        lpop(&stack, &nstack);
    }
    if (libname) {
        mergeuse(ctx.args, ctx.nargs);
        archive(ctx.args, ctx.nargs);
    } else {
        linkobj(ctx.args, ctx.nargs);
    }

    return 0;
}
