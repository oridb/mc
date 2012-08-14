#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "parse.h"

#define Initsz 16

/* Creates a new empty hash table, using 'hash' as the
 * hash funciton, and 'cmp' to verify that there are no
 * hash collisions. */
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

/* Frees a hash table. Passing this function
 * NULL is a no-op. */
void htfree(Htab *ht)
{
    if (!ht)
        return;
    free(ht->keys);
    free(ht->vals);
    free(ht->hashes);
    free(ht);
}

/* Offsets the hash so that '0' can be
 * used as a 'no valid value */
static ulong hash(Htab *ht, void *k)
{
    ulong h;
    h = ht->hash(k);
    if (h == 0)
        return 1;
    else
        return h;
}

/* Resizes the hash table by copying all
 * the old keys into the right slots in a
 * new table. */
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

/* Inserts 'k' into the hash table, possibly
 * killing any previous key that compares
 * as equal. */
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

/* Finds the index that we would insert
 * the key into */
static ssize_t htidx(Htab *ht, void *k)
{
    ssize_t i;
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

/* Looks up a key, returning NULL if
 * the value is not present. Note,
 * if NULL is a valid value, you need
 * to check with hthas() to see if it's
 * not there */
void *htget(Htab *ht, void *k)
{
    ssize_t i;

    i = htidx(ht, k);
    if (i < 0)
        return NULL;
    else
        return ht->vals[i];
}

/* Tests for 'k's presence in 'ht' */
int hthas(Htab *ht, void *k)
{
    return htidx(ht, k) >= 0;
}

/* Returns a list of all keys in the hash
 * table, storing the size of the returned
 * array in 'nkeys'. NB: the value returned
 * is allocated on the heap, and it is the
 * job of the caller to free it */
void **htkeys(Htab *ht, size_t *nkeys)
{
    void **k;
    size_t i, j;

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
    intptr_t h;

    h = (intptr_t) key;
    h *= 357913941;
    h ^= h << 24;
    h += ~357913941;
    h ^= h >> 31;
    h ^= h << 31;
    return h;
}

int ptreq(void *a, void *b)
{
    return a == b;
}
