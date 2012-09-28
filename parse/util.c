#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse.h"

void *zalloc(size_t sz)
{
    void *mem;

    mem = calloc(1, sz);
    if (!mem && sz)
        die("Out of memory");
    return mem;
}


void *xalloc(size_t sz)
{
    void *mem;

    mem = malloc(sz);
    if (!mem && sz)
        die("Out of memory");
    return mem;
}

void *zrealloc(void *mem, size_t oldsz, size_t sz)
{
    char *p;

    p = xrealloc(mem, sz);
    if ((ssize_t)sz - (ssize_t)oldsz > 0)
        bzero(&p[oldsz], sz - oldsz);
    return p;
}

void *xrealloc(void *mem, size_t sz)
{
    mem = realloc(mem, sz);
    if (!mem && sz)
        die("Out of memory");
    return mem;
}

void die(char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

void fatal(int line, char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    fprintf(stdout, "%s:%d: ", filename, line);
    vfprintf(stdout, msg, ap);
    fprintf(stdout, "\n");
    va_end(ap);
    exit(1);
}

/* Some systems don't have strndup. */
char *strdupn(char *s, size_t len)
{
    char *ret;

    ret = xalloc(len + 1);
    memcpy(ret, s, len);
    ret[len] = '\0';
    return ret;
}

char *strjoin(char *u, char *v)
{
    size_t n;
    char *s;

    n = strlen(u) + strlen(v) + 1;
    s = xalloc(n);
    snprintf(s, n + 1, "%s%s", u, v);
    return s;
}

void *memdup(void *mem, size_t len)
{
    void *ret;

    ret = xalloc(len);
    return memcpy(ret, mem, len);
}

void lappend(void *l, size_t *len, void *n)
{
    void ***pl;

    pl = l;
    *pl = xrealloc(*pl, (*len + 1)*sizeof(void*));
    (*pl)[*len] = n;
    (*len)++;
}

void *lpop(void *l, size_t *len)
{
    void ***pl;
    void *v;

    pl = l;
    (*len)--;
    v = (*pl)[*len];
    *pl = xrealloc(*pl, *len * sizeof(void*));
    return v;
}

void linsert(void *p, size_t *len, size_t idx, void *v)
{
    void ***pl, **l;
    size_t i;

    pl = p;
    *pl = xrealloc(*pl, (*len + 1)*sizeof(void*));
    l = *pl;
    for (i = idx; i < *len; i++)
        l[i + 1] = l[i];
    l[idx] = v;
    (*len)++;
}

void ldel(void *l, size_t *len, size_t idx)
{
    void ***pl;
    size_t i;

    assert(l != NULL);
    assert(idx < *len);
    pl = l;
    for (i = idx; i < *len - 1; i++)
	pl[0][i] = pl[0][i + 1];
    (*len)--;
    *pl = xrealloc(*pl, *len * sizeof(void*));
}


void lfree(void *l, size_t *len)
{
    void ***pl;

    assert(l != NULL);
    pl = l;
    free(*pl);
    *pl = NULL;
    *len = 0;
}

void be64(vlong v, byte buf[8])
{
    buf[0] = (v >> 56) & 0xff;
    buf[1] = (v >> 48) & 0xff;
    buf[2] = (v >> 40) & 0xff;
    buf[3] = (v >> 32) & 0xff;
    buf[4] = (v >> 24) & 0xff;
    buf[5] = (v >> 16) & 0xff;
    buf[6] = (v >> 8)  & 0xff;
    buf[7] = (v >> 0)  & 0xff;
}

vlong host64(byte buf[8])
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

void be32(long v, byte buf[4])
{
    buf[0] = (v >> 24) & 0xff;
    buf[1] = (v >> 16) & 0xff;
    buf[2] = (v >> 8)  & 0xff;
    buf[3] = (v >> 0)  & 0xff;
}

long host32(byte buf[4])
{
    int32_t v = 0;
    v |= ((long)buf[0] << 24);
    v |= ((long)buf[1] << 16);
    v |= ((long)buf[2] << 8);
    v |= ((long)buf[3] << 0);
    return v;
}

void wrbuf(FILE *fd, void *buf, size_t sz)
{
    size_t n;

    n = 0;
    while (n < sz) {
	n += fwrite(buf + n, 1, sz - n, fd);
	if (feof(fd))
	    die("Unexpected EOF");
	if (ferror(fd))
	    die("Error writing");
    }
}

void rdbuf(FILE *fd, void *buf, size_t sz)
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

void wrbyte(FILE *fd, char val)
{
    if (fputc(val, fd) == EOF)
        die("Unexpected EOF");
}

char rdbyte(FILE *fd)
{
    int c;
    c = fgetc(fd);
    if (c == EOF)
        die("Unexpected EOF");
    return c;
}

void wrint(FILE *fd, long val)
{
    byte buf[4];
    be32(val, buf);
    wrbuf(fd, buf, 4);
}

long rdint(FILE *fd)
{
    byte buf[4];
    rdbuf(fd, buf, 4);
    return host32(buf);
}

void wrstr(FILE *fd, char *val)
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

char *rdstr(FILE *fd)
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

void wrflt(FILE *fd, double val)
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

double rdflt(FILE *fd)
{
    byte buf[8];
    union {
        uvlong ival;
        double fval;
    } u;

    if (fread(buf, 8, 1, fd) < 8)
        die("Unexpected EOF");
    u.ival = host64(buf);
    return u.fval;
}

void wrbool(FILE *fd, int val)
{
    wrbyte(fd, val);
}

int rdbool(FILE *fd)
{
    return rdbyte(fd);
}

char *swapsuffix(char *buf, size_t sz, char *s, char *suf, char *swap)
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
        strncat(buf, swap, suflen);
    } else {
        snprintf(buf, sz, "%s%s", s, swap);
    }

    return buf;
}

size_t max(size_t a, size_t b)
{
    if (a < b)
        return a;
    else
        return b;
}

size_t min(size_t a, size_t b)
{
    if (a < b)
        return a;
    else
        return b;
}

size_t align(size_t sz, size_t align)
{
    return (sz + align - 1) & ~(align - 1);
}

