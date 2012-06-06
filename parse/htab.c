#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "parse.h"

#define Initsz 16

Htab *mkht(ulong (*hash)(void *key), int (*cmp)(void *k1, void *k2))
{
    Htab *ht;

    ht = xalloc(sizeof(Htab));
    ht->nelt = 0;
    ht->sz = Initsz;
    ht->hash = hash;
    ht->cmp = cmp;
    ht->keys = zalloc(Initsz*sizeof(void*));
    ht->vals = zalloc(Initsz*sizeof(void*));
    ht->hashes = zalloc(Initsz*sizeof(void*));

    return ht;
}

static ulong hash(Htab *ht, void *k)
{
    ulong h;
    h = ht->hash(k);
    if (h == 0)
        return 1;
    else
        return h;
}

static void grow(Htab *ht, int sz)
{
    void **oldk;
    void **oldv;
    ulong *oldh;
    int oldsz;
    int i;

    oldk = ht->keys;
    oldv = ht->vals;
    oldh = ht->hashes;
    oldsz = ht->sz;

    ht->nelt = 0;
    ht->sz = sz;
    ht->keys = zalloc(sz*sizeof(void*));
    ht->vals = zalloc(sz*sizeof(void*));
    ht->hashes = zalloc(sz*sizeof(void*));

    for (i = 0; i < oldsz; i++)
        if (oldh[i])
            htput(ht, oldk[i], oldv[i]);
    free(oldh);
    free(oldk);
    free(oldv);
}

int htput(Htab *ht, void *k, void *v)
{
    int i;
    ulong h;
    int di;

    di = 0;
    h = hash(ht, k);
    i = h & (ht->sz - 1);
    while (ht->hashes[i]) {
        /* second insertion overwrites first */
        if (ht->hashes[i] == h && ht->cmp(ht->keys[i], k))
                break;
        di++;
        i = (h + di) & (ht->sz - 1);
    }
    ht->hashes[i] = h;
    ht->keys[i] = k;
    ht->vals[i] = v;
    ht->nelt++;
    if (ht->sz < ht->nelt*2)
        grow(ht, ht->sz*2);
    return 1;
}

ssize_t htidx(Htab *ht, void *k)
{
    int i;
    ulong h;
    int di;

    di = 0;
    h = hash(ht, k);
    i = h & (ht->sz - 1);
    while (ht->hashes[i] && ht->hashes[i] != h) {
searchmore:
        di++;
        i = (h + di) & (ht->sz - 1);
    }
    if (!ht->hashes[i]) 
        return -1;
    if (!ht->cmp(ht->keys[i], k))
        goto searchmore; /* collision */
    return i;
}

void *htget(Htab *ht, void *k)
{
    ssize_t i;

    i = htidx(ht, k);
    if (i < 0)
        return NULL;
    else
        return ht->vals[i];
}

int hthas(Htab *ht, void *k)
{
    return htidx(ht, k) >= 0;
}

void **htkeys(Htab *ht, int *nkeys)
{
    void **k;
    int i, j;

    j = 0;
    k = xalloc(sizeof(void*)*ht->nelt);
    for (i = 0; i < ht->sz; i++)
        if (ht->hashes[i])
            k[j++] = ht->keys[i];
    *nkeys = ht->nelt;
    return k;
}

ulong strhash(void *_s)
{
    char *s;
    ulong h;
    ulong g;

    s = _s;
    h = 0;
    while (s && *s) {
        h = ((h << 4) + *s++);

        if ((g = (h & 0xF0000000)))
            h ^= (g >> 24);

        h &= ~g;
    }
    return h;
}

int streq(void *a, void *b)
{
    return !strcmp(a, b);
}

ulong ptrhash(void *key)
{
    return (ulong)key;
}

int ptreq(void *a, void *b)
{
    return a == b;
}

