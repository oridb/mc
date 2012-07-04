#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "parse.h"

#define Sizetbits (CHAR_BIT*sizeof(size_t)) /* used in graph reprs */

static void eqsz(Bitset *a, Bitset *b)
{
    int sz;

    if (a->nchunks > b->nchunks)
        sz = a->nchunks;
    else
        sz = b->nchunks;
    a->chunks = zrealloc(a->chunks, a->nchunks*sizeof(size_t), sz*sizeof(size_t));
    a->nchunks = sz;
    b->chunks = zrealloc(b->chunks, a->nchunks*sizeof(size_t), sz*sizeof(size_t));
    b->nchunks = sz;
}

Bitset *mkbs()
{
    Bitset *bs;

    bs = xalloc(sizeof(Bitset));
    bs->nchunks = 1;
    bs->chunks = zalloc(1*sizeof(size_t));
    return bs;
}

void bsfree(Bitset *bs)
{
    free(bs->chunks);
    free(bs);
}

Bitset *bsdup(Bitset *a)
{
    Bitset *bs;

    if (!a)
        return NULL;
    bs = xalloc(sizeof(Bitset));
    bs->nchunks = a->nchunks;
    bs->chunks = xalloc(a->nchunks*sizeof(size_t));
    memcpy(bs->chunks, a->chunks, a->nchunks*sizeof(size_t));
    return bs;
}

Bitset *bsclear(Bitset *bs)
{
    size_t i;

    if (!bs)
        return mkbs();
    for (i = 0; i < bs->nchunks; i++)
        bs->chunks[i] = 0;
    return bs;
}

size_t bscount(Bitset *bs)
{
    size_t i, j, n;

    n = 0;
    for (i = 0; i < bs->nchunks; i++)
        for (j = 0; j < sizeof(size_t)*CHAR_BIT; j++)
            if (bs->chunks[i] & 1ULL << j)
                n++;
    return n;
}

int bsiter(Bitset *bs, size_t *elt)
{
    size_t i;

    for (i = *elt; i < bsmax(bs); i++) {
        while (i < bsmax(bs) && !bs->chunks[i/Sizetbits])
            i = (i + Sizetbits) & ~(Sizetbits - 1);
        if (bshas(bs, i)) {
            *elt = i;
            return 1;
        }
    }
    return 0;
}

/* Returns the largest value that the bitset can possibly
 * hold. It's conservative, but scanning the entire bitset
 * is a bit slow. This is mostly an aid to iterate over it. */
size_t bsmax(Bitset *bs)
{
    return bs->nchunks*Sizetbits;
}

void delbs(Bitset *bs)
{
    free(bs->chunks);
    free(bs);
}

void bsput(Bitset *bs, size_t elt)
{
    size_t sz;
    if (elt >= bs->nchunks*Sizetbits) {
        sz = (elt/Sizetbits)+1;
        bs->chunks = zrealloc(bs->chunks, bs->nchunks*sizeof(size_t), sz*sizeof(size_t));
        bs->nchunks = sz;
    }
    bs->chunks[elt/Sizetbits] |= 1ULL << (elt % Sizetbits);
}

void bsdel(Bitset *bs, size_t elt)
{
    if (elt < bs->nchunks*Sizetbits)
        bs->chunks[elt/Sizetbits] &= ~(1ULL << (elt % Sizetbits));
}

int bshas(Bitset *bs, size_t elt)
{
    if (elt >= bs->nchunks*Sizetbits)
        return 0;
    else
        return bs->chunks[elt/Sizetbits] & (1ULL << (elt % Sizetbits));
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

int bseq(Bitset *a, Bitset *b)
{
    size_t i;

    eqsz(a, b);
    for (i = 0; i < a->nchunks; i++)
        if (a->chunks[i] != b->chunks[i])
            return 0;
    return 1;
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
