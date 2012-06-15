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

void *memdup(void *mem, size_t len)
{
    void *ret;

    ret = xalloc(len);
    return memcpy(ret, mem, len);
}

void lappend(void *l, size_t *len, void *n)
{
    void ***pl;

    assert(n != NULL);
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

void ldel(void *l, size_t *len, size_t idx)
{
    void ***pl;
    size_t i;

    assert(l != NULL);
    assert(idx < *len);
    pl = l;
    for (i = idx; i < *len - 1; i++)
	pl[i] = pl[i + 1];
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
