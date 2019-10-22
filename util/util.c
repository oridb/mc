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

/* Some systems don't have strndup. */
char *
strdupn(char *s, size_t len)
{
	char *ret;

	ret = xalloc(len + 1);
	memcpy(ret, s, len);
	ret[len] = '\0';
	return ret;
}

char *
xstrdup(char *s)
{
	char *p;

	p = strdup(s);
	if (!p && s)
		die("Out of memory");
	return p;
}

char *
strjoin(char *u, char *v)
{
	size_t n;
	char *s;

	n = strlen(u) + strlen(v) + 1;
	s = xalloc(n);
	bprintf(s, n + 1, "%s%s", u, v);
	return s;
}

char *
swapsuffix(char *buf, size_t sz, char *s, char *suf, char *swap)
{
	size_t slen, suflen, swaplen;

	slen = strlen(s);
	suflen = strlen(suf);
	swaplen = strlen(swap);

	if (slen < suflen)
		return NULL;
	if (slen + swaplen >= sz)
		die("swapsuffix: buf too small");

	buf[0] = '\0';
	/* if we have matching suffixes */
	if (suflen < slen && !strcmp(suf, &s[slen - suflen])) {
		strncat(buf, s, slen - suflen);
		strncat(buf, swap, swaplen);
	} else {
		bprintf(buf, sz, "%s%s", s, swap);
	}

	return buf;
}

size_t
max(size_t a, size_t b)
{
	if (a > b)
		return a;
	else
		return b;
}

size_t
min(size_t a, size_t b)
{
	if (a < b)
		return a;
	else
		return b;
}

size_t
align(size_t sz, size_t a)
{
	/* align to 0 just returns sz */
	if (a == 0)
		return sz;
	return (sz + a - 1) & ~(a - 1);
}

void
indentf(int depth, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfindentf(stdout, depth, fmt, ap);
	va_end(ap);
}

void
findentf(FILE *fd, int depth, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfindentf(fd, depth, fmt, ap);
	va_end(ap);
}

void
vfindentf(FILE *fd, int depth, char *fmt, va_list ap)
{
	ssize_t i;

	for (i = 0; i < depth; i++)
		fprintf(fd, "\t");
	vfprintf(fd, fmt, ap);
}

static int
optinfo(Optctx *ctx, char arg, int *take, int *mand)
{
	char *s;

	for (s = ctx->optstr; *s != '\0'; s++) {
		if (*s == arg) {
			s++;
			if (*s == ':') {
				*take = 1;
				*mand = 1;
				return 1;
			} else if (*s == '?') {
				*take = 1;
				*mand = 0;
				return 1;
			} else {
				*take = 0;
				*mand = 0;
				return 1;
			}
		}
	}
	return 0;
}

static int
findnextopt(Optctx *ctx)
{
	size_t i;

	for (i = ctx->argidx + 1; i < ctx->noptargs; i++) {
		if (ctx->optargs[i][0] == '-')
			goto foundopt;
		else
			lappend(&ctx->args, &ctx->nargs, ctx->optargs[i]);
	}
	ctx->finished = 1;
	return 0;
foundopt
:
	ctx->argidx = i;
	ctx->curarg = ctx->optargs[i] + 1; /* skip initial '-' */
	return 1;
}

void
optinit(Optctx *ctx, char *optstr, char **optargs, size_t noptargs)
{
	ctx->args = NULL;
	ctx->nargs = 0;

	ctx->optstr = optstr;
	ctx->optargs = optargs;
	ctx->noptargs = noptargs;
	ctx->optdone = 0;
	ctx->finished = 0;
	ctx->argidx = 0;
	ctx->curarg = "";
	findnextopt(ctx);
}

int
optnext(Optctx *ctx)
{
	int take, mand;
	int c;

	c = *ctx->curarg;
	ctx->curarg++;
	if (!optinfo(ctx, c, &take, &mand)) {
		printf("Unexpected argument %c\n", c);
		exit(1);
	}

	ctx->optarg = NULL;
	if (take) {
		if (*ctx->curarg) {
			ctx->optarg = ctx->curarg;
			ctx->curarg += strlen(ctx->optarg);
		} else if (ctx->argidx < ctx->noptargs - 1) {
			ctx->optarg = ctx->optargs[ctx->argidx + 1];
			ctx->argidx++;
		} else if (mand) {
			fprintf(stderr, "expected argument for %c\n", *ctx->curarg);
		}
		findnextopt(ctx);
	} else {
		if (*ctx->curarg == '\0')
			findnextopt(ctx);
	}
	return c;
}

int
optdone(Optctx *ctx) { return *ctx->curarg == '\0' && ctx->finished; }
