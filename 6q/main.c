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

#include "util.h"
#include "parse.h"
#include "mi.h"
#include "qasm.h"

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
	printf("%s [-?|-h] [-o outfile] [-d[dbgopts]] inputs\n", prog);
	printf("\t-?|-h\tPrint this help\n");
	printf("\t-o\tOutput to outfile\n");
	printf("\t-S\tGenerate assembly source alongside object code\n");
	printf("\t-I path\tAdd 'path' to use search path\n");
	printf("\t-d opts: additional debug logging. Options are listed below:\n");
	printf("\t\tf: log folded trees\n");
	printf("\t\tl: log lowered pre-cfg trees\n");
	printf("\t\tT: log tree immediately\n");
	printf("\t\tr: log register allocation activity\n");
	printf("\t\ti: log instruction selection activity\n");
	printf("\t\tu: log type unifications\n");
}

static void swapout(char* buf, size_t sz, char* suf) {
	char* psuffix;
	psuffix = strrchr(outfile, '.');
	if (psuffix != NULL)
		swapsuffix(buf, sz, outfile, psuffix, suf);
	else
		bprintf(buf, sz, "%s%s", outfile, suf);
}

static void assemble(char *asmsrc, char *path)
{
	char *asmcmd[] = {"/home/ori/myr/qbe/obj/qbe", "-o", NULL};
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

	for (p = cmd; *p; p++)
		printf("%s ", *p);
	printf("\n");

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

static char *gentempfile(char *buf, size_t bufsz, char *path, char *suffix)
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
	bprintf(buf, bufsz, "%s/tmp%lx%lx-%s%s", tmpdir, (long)tv.tv_sec, (long)tv.tv_usec, base, suffix);
	return buf;
}

static int hasmain(Node *file)
{
	Node *n, *name;

	name = mknsname(Zloc, "", "main");
	n = getdcl(file->file.globls, name);
	if (!n)
		return 0;
	n = n->decl.name;
	if (n->name.ns)
		return 0;
	return 1;
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

	optinit(&ctx, "cd:?hSo:I:9G:", argv, argc);
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
		case 'd':
			while (ctx.optarg && *ctx.optarg)
				debugopt[*ctx.optarg++ & 0x7f]++;
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
		globls = mkstab(0);
		tyinit(globls);
		tokinit(ctx.args[i]);
		file = mkfile(ctx.args[i]);
		file->file.globls = globls;
		yyparse();

		/* before we do anything to the parse */
		if (debugopt['T'])
			dump(file, stdout);
		infer(file);
		if (hasmain(file))
			geninit(file);
		tagexports(file, 0);
		/* after all type inference */
		if (debugopt['t'])
			dump(file, stdout);

		if (writeasm) {
			if (outfile != NULL)
				swapout(buf, sizeof buf, ".s");
			else
				swapsuffix(buf, sizeof buf, ctx.args[i], ".myr", ".s");
		} else {
			gentempfile(buf, sizeof buf, ctx.args[i], ".s");
		}
		genuse(ctx.args[i]);
		gen(file, buf);
		assemble(buf, ctx.args[i]);
	}

	return 0;
}
