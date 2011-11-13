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
    if (!mem)
        die("Out of memory");
    return mem;
}


void *xalloc(size_t sz)
{
    void *mem;

    mem = malloc(sz);
    if (!mem)
        die("Out of memory");
    return mem;
}

void *xrealloc(void *mem, size_t sz)
{
    mem = realloc(mem, sz);
    if (!mem)
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
    fprintf(stderr, "%s:%d: ", filename, line);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

/* Some systems don't have strndup. */
char *strdupn(char *s, size_t len)
{
    char *ret;

    ret = xalloc(len);
    memcpy(ret, s, len);
    return ret;
}
