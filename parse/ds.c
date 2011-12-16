#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>

#include "parse.h"

#define Uintbits (CHAR_BIT*sizeof(int))

Bitset *mkbs()
{
    Bitset *bs;

    bs = xalloc(sizeof(Bitset));
    bs->nchunks = 1;
    bs->chunks = xalloc(1*sizeof(uint));
    return bs;
}

void delbs(Bitset *bs)
{
    free(bs->chunks);
    free(bs);
}

void bsput(Bitset *bs, uint elt)
{
    if (elt >= bs->nchunks*Uintbits) {
        bs->nchunks = (elt/Uintbits)+1;
        bs->chunks = xrealloc(bs->chunks, bs->nchunks*sizeof(uint));
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


