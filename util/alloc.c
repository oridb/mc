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
/* malloc wrappers */
void *
zalloc(size_t sz)
{
	void *mem;

	mem = calloc(1, sz);
	if (!mem && sz)
		die("Out of memory");
	return mem;
}

void *
xalloc(size_t sz)
{
	void *mem;

	mem = malloc(sz);
	if (!mem && sz)
		die("Out of memory");
	return mem;
}

void *
zrealloc(void *mem, size_t oldsz, size_t sz)
{
	char *p;

	p = xrealloc(mem, sz);
	if (sz > oldsz)
		memset(&p[oldsz], 0, sz - oldsz);
	return p;
}

void *
xrealloc(void *mem, size_t sz)
{
	mem = realloc(mem, sz);
	if (!mem && sz)
		die("Out of memory");
	return mem;
}

/* lists */
void
lappend(void *l, size_t *len, void *n)
{
	void ***pl;

	pl = l;
	*pl = xrealloc(*pl, (*len + 1) * sizeof(void *));
	(*pl)[*len] = n;
	(*len)++;
}

void *
lpop(void *l, size_t *len)
{
	void ***pl;
	void *v;

	pl = l;
	(*len)--;
	v = (*pl)[*len];
	*pl = xrealloc(*pl, *len * sizeof(void *));
	return v;
}

void
linsert(void *p, size_t *len, size_t idx, void *v)
{
	void ***pl, **l;

	pl = p;
	*pl = xrealloc(*pl, (*len + 1) * sizeof(void *));
	l = *pl;

	memmove(&l[idx + 1], &l[idx], (*len - idx) * sizeof(void *));
	l[idx] = v;
	(*len)++;
}

void
ldel(void *p, size_t *len, size_t idx)
{
	void ***pl, **l;

	assert(p != NULL);
	assert(idx < *len);
	pl = p;
	l = *pl;
	memmove(&l[idx], &l[idx + 1], (*len - idx - 1) * sizeof(void *));
	(*len)--;
	*pl = xrealloc(l, *len * sizeof(void *));
}

void
lcat(void *dst, size_t *ndst, void *src, size_t nsrc)
{
	size_t i;
	void ***d, **s;

	d = dst;
	s = src;
	for (i = 0; i < nsrc; i++)
		lappend(d, ndst, s[i]);
}

void
lfree(void *l, size_t *len)
{
	void ***pl;

	assert(l != NULL);
	pl = l;
	free(*pl);
	*pl = NULL;
	*len = 0;
}

void *
memdup(void *mem, size_t len)
{
	void *ret;

        if (!mem)
            return NULL;
	ret = xalloc(len);
	return memcpy(ret, mem, len);
}
