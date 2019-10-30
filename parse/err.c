#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "parse.h"

/* errors */
void
die(char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	abort();
}

void
fatal(Node *n, char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	lfatalv(n->loc, msg, ap);
	va_end(ap);
}

void
lfatal(Srcloc l, char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	lfatalv(l, msg, ap);
	va_end(ap);
}

void
lfatalv(Srcloc l, char *msg, va_list ap)
{
	fprintf(stdout, "%s:%d: ", fname(l), lnum(l));
	vfprintf(stdout, msg, ap);
	fprintf(stdout, "\n");
	exit(1);
}
