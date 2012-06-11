#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "parse.h"

#define Uintbits (CHAR_BIT*sizeof(int))

static void eqsz(Bitset *a, Bitset *b)
{
    int sz;

    if (a->nchunks > b->nchunks)
        sz = a->nchunks;
    else
        sz = b->nchunks;
    a->chunks = zrealloc(a->chunks, a->nchunks*sizeof(uint), sz*sizeof(uint));
    a->nchunks = sz;
    b->chunks = zrealloc(b->chunks, a->nchunks*sizeof(uint), sz*sizeof(uint));
    b->nchunks = sz;
}

Bitset *mkbs()
{
    Bitset *bs;

    bs = xalloc(sizeof(Bitset));
    bs->nchunks = 1;
    bs->chunks = zalloc(1*sizeof(uint));
    return bs;
}

Bitset *dupbs(Bitset *a)
{
    Bitset *bs;

    bs = xalloc(sizeof(Bitset));
    bs->nchunks = a->nchunks;
    bs->chunks = xalloc(a->nchunks*sizeof(uint));
    memcpy(bs->chunks, a->chunks, a->nchunks*sizeof(uint));
    return bs;
}

size_t bscount(Bitset *bs)
{
    size_t i, j, n;

    n = 0;
    for (i = 0; i < bs->nchunks; i++)
        for (j = 1; j < sizeof(uint)*CHAR_BIT; j++)
            if (bs->chunks[i] & 1 << j)
                n++;
    return n;
}

/* Returns the largest value that the bitset can possibly
 * hold. It's conservative, but scanning the entire bitset
 * is a bit slow. This is mostly an aid to iterate over it. */
size_t bsmax(Bitset *bs)
{
    return bs->nchunks*sizeof(uint)*CHAR_BIT;
}

void delbs(Bitset *bs)
{
    free(bs->chunks);
    free(bs);
}

void bsput(Bitset *bs, uint elt)
{
    size_t sz;
    if (elt >= bs->nchunks*Uintbits) {
        sz = (elt/Uintbits)+1;
        bs->chunks = zrealloc(bs->chunks, bs->nchunks*sizeof(uint), sz*sizeof(uint));
        bs->nchunks = sz;
    }
    bs->chunks[elt/Uintbits] |= 1 << (elt % Uintbits);
}

void bsdel(Bitset *bs, uint elt)
{
    if (elt < bs->nchunks*Uintbits)
        bs->chunks[elt/Uintbits] &= ~(1 << (elt % Uintbits));
}

int bshas(Bitset *bs, uint elt)
{
    if (elt >= bs->nchunks*Uintbits)
        return 0;
    else
        return bs->chunks[elt/Uintbits] & (1 << (elt % Uintbits));
}

void bsunion(Bitset *a, Bitset *b)
{
    size_t i;

    eqsz(a, b);
    for (i = 0; i < a->nchunks; i++)
        a->chunks[i] |= b->chunks[i];
}

void bsintersect(Bitset *a, Bitset *b)
{
    size_t i;

    eqsz(a, b);
    for (i = 0; i < a->nchunks; i++)
        a->chunks[i] &= b->chunks[i];
}

void bsdiff(Bitset *a, Bitset *b)
{
    size_t i;

    eqsz(a, b);
    for (i = 0; i < a->nchunks; i++)
        a->chunks[i] &= ~b->chunks[i];
}

int bsissubset(Bitset *set, Bitset *sub)
{
    size_t i;

    eqsz(set, sub);
    for (i = 0; i < set->nchunks; i++)
        if ((sub->chunks[i] & set->chunks[i]) != set->chunks[i])
            return 0;
    return 1;
}
