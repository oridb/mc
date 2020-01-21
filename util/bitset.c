#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "util.h"

#define Sizetbits (CHAR_BIT * sizeof(size_t)) /* used in graph reprs */

/* Equalizes the size of a and b by
 * growing the smaller to the size of the
 * larger, zeroing out the new elements.
 * This allows the code to simply iterate
 * over both without keeping track of the
 * minimum size.
 */
static void
eqsz(Bitset *a, Bitset *b)
{
	size_t sz;
	size_t i;
	size_t *p;

	if (a->nchunks > b->nchunks)
		sz = a->nchunks;
	else
		sz = b->nchunks;

	if (a->nchunks != sz) {
		p = zalloc(sz * sizeof(size_t));
		for (i = 0; i < a->nchunks; i++)
			p[i] = a->chunks[i];
		free(a->chunks);
		a->chunks = p;
		a->nchunks = sz;
	}

	if (b->nchunks != sz) {
		p = zalloc(sz * sizeof(size_t));
		for (i = 0; i < b->nchunks; i++)
			p[i] = b->chunks[i];
		free(b->chunks);
		b->chunks = p;
		b->nchunks = sz;
	}
}

/* Creates a new all-zero bit set */
Bitset *
mkbs()
{
	Bitset *bs;

	bs = xalloc(sizeof(Bitset));
	bs->nchunks = 1;
	bs->chunks = zalloc(1 * sizeof(size_t));
	return bs;
}

/* Frees a bitset. Safe to call on NULL. */
void
bsfree(Bitset *bs)
{
	if (!bs)
		return;
	free(bs->chunks);
	free(bs);
}

/* Duplicates a bitset. NULL is duplicated to NULL. */
Bitset *
bsdup(Bitset *a)
{
	Bitset *bs;

	if (!a)
		return NULL;
	bs = xalloc(sizeof(Bitset));
	bs->nchunks = a->nchunks;
	bs->chunks = xalloc(a->nchunks * sizeof(size_t));
	memcpy(bs->chunks, a->chunks, a->nchunks * sizeof(size_t));
	return bs;
}

/* Zeroes all values in a bit set */
Bitset *
bsclear(Bitset *bs)
{
	size_t i;

	if (!bs)
		return mkbs();
	for (i = 0; i < bs->nchunks; i++)
		bs->chunks[i] = 0;
	return bs;
}

/* Counts the number of values held in a bit set */
size_t
bscount(Bitset *bs)
{
	size_t i, j, n;

	n = 0;
	for (i = 0; i < bs->nchunks; i++)
		for (j = 0; j < sizeof(size_t) * CHAR_BIT; j++)
			if (bs->chunks[i] & 1ULL << j)
				n++;
	return n;
}

static inline int
firstbit(uint64_t b)
{
	int n;
	static const char bits[] = {
		4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0
	};

	n = 0;
	if (!(b & 0xffffffff)) {
		n += 32;
		b >>= 32;
	}
	if (!(b & 0xffff)) {
		n += 16;
		b >>= 16;
	}
	if (!(b & 0xff)) {
		n += 8;
		b >>= 8;
	}
	if (!(b & 0xf)) {
		n += 4;
		b >>= 4;
	}
	n += bits[b & 0xf];
	return n;
}

/* A slightly tricky function to iterate over the content
 * of a bitset. It returns true immediately if 'elt' is in
 * the bitset, otherwise it seeks forward to the next value
 * held in the bitset and stores it in elt. If there are no
 * more values, it returns false to stop iteration. Note,
 * this means that you need to increment elt every time you
 * pass through.
 *
 * Typical usage of this function:
 *
 *	  for (i = 0; bsiter(set, &i); i++)
 *		  use(i);
 *
 * The increment of 'i' in the for loop is needed in order
 * to prevent the function from returning the same value
 * repeatedly.
 */
int
bsiter(Bitset *bs, size_t *elt)
{
	size_t b, t, i;

	i = *elt;
	t = i/Sizetbits;
	if (t >= bs->nchunks)
		return 0;
	b = bs->chunks[t];
	b &= ~((1ull << (i%Sizetbits)) - 1);
	while (!b) {
		++t;
		if (t >= bs->nchunks)
			return 0;
		b = bs->chunks[t];
	}
	*elt = Sizetbits*t + firstbit(b);
	return 1;
}


/* Returns the largest value that the bitset can possibly
 * hold. It's conservative, but scanning the entire bitset
 * is a bit slow. This is mostly an aid to iterate over it. */
size_t
bsmax(Bitset *bs) { return bs->nchunks * Sizetbits; }

void
bsput(Bitset *bs, size_t elt)
{
	size_t sz;
	if (elt >= bs->nchunks * Sizetbits) {
		sz = (elt / Sizetbits) + 1;
		bs->chunks = zrealloc(bs->chunks, bs->nchunks * sizeof(size_t), sz * sizeof(size_t));
		bs->nchunks = sz;
	}
	bs->chunks[elt / Sizetbits] |= 1ULL << (elt % Sizetbits);
}

void
bsdel(Bitset *bs, size_t elt)
{
	if (elt < bs->nchunks * Sizetbits)
		bs->chunks[elt / Sizetbits] &= ~(1ULL << (elt % Sizetbits));
}

void
bsunion(Bitset *a, Bitset *b)
{
	size_t i;

	eqsz(a, b);
	for (i = 0; i < a->nchunks; i++)
		a->chunks[i] |= b->chunks[i];
}

void
bsintersect(Bitset *a, Bitset *b)
{
	size_t i;

	eqsz(a, b);
	for (i = 0; i < a->nchunks; i++)
		a->chunks[i] &= b->chunks[i];
}

ulong
bshash(Bitset *bs)
{
	if (!bs)
		return 14247517;
	else
		return murmurhash2(bs->chunks, bs->nchunks * sizeof(size_t));
}

void
bsdiff(Bitset *a, Bitset *b)
{
	size_t i;

	eqsz(a, b);
	for (i = 0; i < a->nchunks; i++)
		a->chunks[i] &= ~b->chunks[i];
}

int
bseq(Bitset *a, Bitset *b)
{
	size_t i;

	if (!a || !b)
		return bsisempty(a) && bsisempty(b);
	eqsz(a, b);
	for (i = 0; i < a->nchunks; i++) {
		if (a->chunks[i] != b->chunks[i])
			return 0;
	}
	return 1;
}

int
bsissubset(Bitset *set, Bitset *sub)
{
	size_t i;

	eqsz(set, sub);
	for (i = 0; i < set->nchunks; i++)
		if ((sub->chunks[i] & set->chunks[i]) != set->chunks[i])
			return 0;
	return 1;
}

int
bsisempty(Bitset *set)
{
	size_t i;

	if (!set)
		return 1;
	for (i = 0; i < set->nchunks; i++)
		if (set->chunks[i])
			return 0;
	return 1;
}
