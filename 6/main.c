#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "parse.h"
#include "mi.h"
#include "asm.h"

#include "../config.h"

/* FIXME: move into one place...? */
Node *file;
char debugopt[128];
int writeasm;
int extracheck;
int p9asm;
char *outfile;
char **incpaths;
size_t nincpaths;
Asmsyntax asmsyntax;

static void usage(char *prog)
{
    printf("%s [-?] [-o outfile] [-d[dbgopts]] inputs\n", prog);
    printf("\t-?\tPrint this help\n");
    printf("\t-o\tOutput to outfile\n");
    printf("\t-S\tGenerate assembly source alongside object code\n");
    printf("\t-c\tEnable additional (possibly flaky) checking\n");
    printf("\t-I path\tAdd 'path' to use search path\n");
    printf("\t-d\tPrint debug dumps. Recognized options: f r p i\n");
    printf("\t-G\tGenerate asm in gas syntax\n");
    printf("\t-8\tGenerate asm in plan 9 syntax\n");
    printf("\t\t\tf: log folded trees\n");
    printf("\t\t\tl: log lowered pre-cfg trees\n");
    printf("\t\t\tT: log tree immediately\n");
    printf("\t\t\tr: log register allocation activity\n");
    printf("\t\t\ti: log instruction selection activity\n");
    printf("\t\t\tu: log type unifications\n");
}

static void swapout(char* buf, size_t sz, char* suf) {
    char* psuffix;
    psuffix = strrchr(outfile, '.');
    if (psuffix != NULL)
        swapsuffix(buf, sz, outfile, psuffix, suf);
    else
        snprintf(buf, sz, "%s%s", outfile, suf);
}

static void assemble(char *asmsrc, char *path)
{
    char *asmcmd[] = Asmcmd;
    char objfile[1024];
    char *psuffix;
    char **p, **cmd;
    size_t ncmd;
    int pid, status;

    if (outfile != NULL)
        strncpy(objfile, outfile, 1024);
    else {
        psuffix = strrchr(path, '+');
        if (psuffix != NULL)
            swapsuffix(objfile, 1024, path, psuffix, Objsuffix);
        else
            swapsuffix(objfile, 1024, path, ".myr", Objsuffix);
    }
    cmd = NULL;
    ncmd = 0;
    for (p = asmcmd; *p != NULL; p++)
        lappend(&cmd, &ncmd, *p);
    lappend(&cmd, &ncmd, objfile);
    lappend(&cmd, &ncmd, asmsrc);
    lappend(&cmd, &ncmd, NULL);

    pid = fork();
    if (pid == -1) {
        die("couldn't fork");
    } else if (pid == 0) {
        if (execvp(cmd[0], cmd) == -1)
            die("Couldn't exec assembler\n");
    } else {
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            die("Couldn't run assembler");
    }
}

static char *gentemp(char *buf, size_t bufsz, char *path, char *suffix)
{
    char *tmpdir;
    char *base;
    struct timeval tv;

    tmpdir = getenv("TMPDIR");
    if (!tmpdir)
        tmpdir = "/tmp";
    base = strrchr(path, '/');
    if (base)
        base++;
    else
        base = path;
    gettimeofday(&tv, NULL);
    snprintf(buf, bufsz, "%s/tmp%lx%lx-%s%s", tmpdir, (long)tv.tv_sec, (long)tv.tv_usec, base, suffix);
    return buf;
}

static void genuse(char *path)
{
    FILE *f;
    char buf[1024];
    char *psuffix;

    if (outfile != NULL)
        swapout(buf, 1024, ".use");
    else {
        psuffix = strrchr(path, '+');
        if (psuffix != NULL)
            swapsuffix(buf, 1024, path, psuffix, ".use");
        else
            swapsuffix(buf, 1024, path, ".myr", ".use");
    }
    f = fopen(buf, "w");
    if (!f) {
        fprintf(stderr, "Could not open path %s\n", buf);
        exit(1);
    }
    writeuse(f, file);
    fclose(f);
}

int main(int argc, char **argv)
{
    char buf[1024];
    Stab *globls;
    Optctx ctx;
    size_t i;

    outfile = NULL;

    optinit(&ctx, "d:hSo:I:9G", argv, argc);
    asmsyntax = Defaultasm;
    while (!optdone(&ctx)) {
        switch (optnext(&ctx)) {
            case 'o':
                outfile = ctx.optarg;
                break;
            case 'S':
                writeasm = 1;
                break;
            case '?':
            case 'h':
                usage(argv[0]);
                exit(0);
                break;
            case 'c':
                extracheck = 1;
            case 'd':
                while (ctx.optarg && *ctx.optarg)
                    debugopt[*ctx.optarg++ & 0x7f]++;
                break;
            case '9':
                asmsyntax = Plan9;
                break;
            case 'G':
                asmsyntax = Gnugas;
                break;
            case 'I':
                lappend(&incpaths, &nincpaths, ctx.optarg);
                break;
            default:
                usage(argv[0]);
                exit(0);
                break;
        }
    }

    lappend(&incpaths, &nincpaths, Instroot "/lib/myr");

    if (ctx.nargs == 0) {
        fprintf(stderr, "No input files given\n");
        exit(1);
    }
    else if (ctx.nargs > 1)
        outfile = NULL;

    for (i = 0; i < ctx.nargs; i++) {
        globls = mkstab();
        tyinit(globls);
        tokinit(ctx.args[i]);
        file = mkfile(ctx.args[i]);
        file->file.globls = globls;
        yyparse();

        /* before we do anything to the parse */
        if (debugopt['T'])
            dump(file, stdout);
        infer(file);
        tagexports(file->file.globls, 0);
        /* after all type inference */
        if (debugopt['t'])
            dump(file, stdout);

        if (writeasm) {
            if (outfile != NULL)
                swapout(buf, sizeof buf, ".s");
            else
                swapsuffix(buf, sizeof buf, ctx.args[i], ".myr", ".s");
        } else {
            gentemp(buf, sizeof buf, ctx.args[i], ".s");
        }
        gen(file, buf);
        assemble(buf, ctx.args[i]);
        genuse(ctx.args[i]);
    }

    return 0;
}
