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

/* endian packing */
void
be64(vlong v, byte buf[8])
{
	buf[0] = (v >> 56) & 0xff;
	buf[1] = (v >> 48) & 0xff;
	buf[2] = (v >> 40) & 0xff;
	buf[3] = (v >> 32) & 0xff;
	buf[4] = (v >> 24) & 0xff;
	buf[5] = (v >> 16) & 0xff;
	buf[6] = (v >> 8) & 0xff;
	buf[7] = (v >> 0) & 0xff;
}

vlong
host64(byte buf[8])
{
	vlong v = 0;

	v |= ((vlong)buf[0] << 56LL);
	v |= ((vlong)buf[1] << 48LL);
	v |= ((vlong)buf[2] << 40LL);
	v |= ((vlong)buf[3] << 32LL);
	v |= ((vlong)buf[4] << 24LL);
	v |= ((vlong)buf[5] << 16LL);
	v |= ((vlong)buf[6] << 8LL);
	v |= ((vlong)buf[7] << 0LL);
	return v;
}

void
be32(long v, byte buf[4])
{
	buf[0] = (v >> 24) & 0xff;
	buf[1] = (v >> 16) & 0xff;
	buf[2] = (v >> 8) & 0xff;
	buf[3] = (v >> 0) & 0xff;
}

long
host32(byte buf[4])
{
	int32_t v = 0;
	v |= ((long)buf[0] << 24);
	v |= ((long)buf[1] << 16);
	v |= ((long)buf[2] << 8);
	v |= ((long)buf[3] << 0);
	return v;
}

void
wrbuf(FILE *fd, void *p, size_t sz)
{
	size_t n;
	char *buf;

	n = 0;
	buf = p;
	while (n < sz) {
		n += fwrite(buf + n, 1, sz - n, fd);
		if (feof(fd))
			die("Unexpected EOF");
		if (ferror(fd))
			die("Error writing");
	}
}

void
rdbuf(FILE *fd, void *buf, size_t sz)
{
	size_t n;

	n = sz;
	while (n > 0) {
		n -= fread(buf, 1, n, fd);
		if (feof(fd))
			die("Unexpected EOF");
		if (ferror(fd))
			die("Error writing");
	}
}

void
wrbyte(FILE *fd, char val)
{
	if (fputc(val, fd) == EOF)
		die("Unexpected EOF");
}

char
rdbyte(FILE *fd)
{
	int c;
	c = fgetc(fd);
	if (c == EOF)
		die("Unexpected EOF");
	return c;
}

void
wrint(FILE *fd, long val)
{
	byte buf[4];
	be32(val, buf);
	wrbuf(fd, buf, 4);
}

long
rdint(FILE *fd)
{
	byte buf[4];
	rdbuf(fd, buf, 4);
	return host32(buf);
}

void
wrstr(FILE *fd, char *val)
{
	size_t len;

	if (!val) {
		wrint(fd, -1);
	} else {
		wrint(fd, strlen(val));
		len = strlen(val);
		wrbuf(fd, val, len);
	}
}

char *
rdstr(FILE *fd)
{
	ssize_t len;
	char *s;

	len = rdint(fd);
	if (len == -1) {
		return NULL;
	} else {
		s = xalloc(len + 1);
		rdbuf(fd, s, len);
		s[len] = '\0';
		return s;
	}
}

void
wrlenstr(FILE *fd, Str str)
{
	wrint(fd, str.len);
	wrbuf(fd, str.buf, str.len);
}

void
rdlenstr(FILE *fd, Str *str)
{
	str->len = rdint(fd);
	str->buf = xalloc(str->len + 1);
	rdbuf(fd, str->buf, str->len);
	str->buf[str->len] = '\0';
}

void
wrflt(FILE *fd, double val)
{
	byte buf[8];
	/* Assumption: We have 'val' in 64 bit IEEE format */
	union {
		uvlong ival;
		double fval;
	} u;

	u.fval = val;
	be64(u.ival, buf);
	wrbuf(fd, buf, 8);
}

double
rdflt(FILE *fd)
{
	byte buf[8];
	union {
		uvlong ival;
		double fval;
	} u;

	rdbuf(fd, buf, 8);
	u.ival = host64(buf);
	return u.fval;
}

size_t
bprintf(char *buf, size_t sz, char *fmt, ...)
{
	va_list ap;
	size_t n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sz, fmt, ap);
	if (n >= sz)
		n = sz;
	va_end(ap);

	return n;
}

void
wrbool(FILE *fd, int val)
{
	wrbyte(fd, val);
}

int
rdbool(FILE *fd)
{
	return rdbyte(fd);
}
