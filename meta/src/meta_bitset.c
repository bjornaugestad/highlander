/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include <meta_common.h>
#include <meta_bitset.h>

/*
 * Implementation of the bitset ADT.
 * The bits are stored in an array of unsigned char, one bit per bit(Duh!)
 */
struct bitset_tag {
	size_t bitcount, size;
	unsigned char *data;
};

bitset bitset_new(size_t bitcount)
{
	bitset b;
	size_t size = bitcount / CHAR_BIT + (bitcount % CHAR_BIT ? 1 : 0);

	if ((b = calloc(1, sizeof *b)) == NULL)
		;
	else if ((b->data = malloc(size)) == NULL) {
		free(b);
		b = NULL;
	}
	else {
		b->size = size;
		b->bitcount = bitcount;
		bitset_clear_all(b);
	}

	return b;
}

bitset bitset_dup(bitset b)
{
	bitset new;

	assert(b != NULL);

	if ((new = bitset_new(b->bitcount)) == NULL)
		return NULL;

	memcpy(new->data, b->data, new->size);
	return new;
}

void bitset_free(bitset b)
{
	free(b->data);
	free(b);
}

bitset bitset_map(void *data, size_t elemcount)
{
	bitset b;

	assert(data != NULL);

	if ((b = calloc(1, sizeof *b)) != NULL) {
		b->size = elemcount;
		b->data = data;
	}

	return b;
}

void bitset_unmap(bitset b)
{
	free(b);
}

void bitset_set(bitset b, size_t i)
{
	assert(b != NULL);
	assert(i < b->size * CHAR_BIT);

	b->data[i / CHAR_BIT] |= (1 << (i % CHAR_BIT));
}

int bitset_is_set(const bitset b, size_t i)
{
	assert(b != NULL);
	assert(i < b->size * CHAR_BIT);

	return b->data[i / CHAR_BIT] & (1 << (i % CHAR_BIT));
}

int bitset_allzero(const bitset b)
{
	size_t i;

	assert(b != NULL);

	for (i = 0; i < b->size; i++) 
		if (b->data[i] != 0)
			return 0;

	return 1;
}

int bitset_allone(const bitset b)
{
	size_t i;

	assert(b != NULL);

	for (i = 0; i < b->size; i++) 
		if (b->data[i] != 0xff)
			return 0;

	return 1;
}

void bitset_clear_all(bitset b)
{
	assert(b != NULL);
	memset(b->data, '\0', b->size);
}

void bitset_set_all(bitset b)
{
	assert(b != NULL);
	memset(b->data, 0xff, b->size);
}

size_t bitset_size(bitset b)
{
	assert(b != NULL);
	return b->size;
}

size_t bitset_bitcount(bitset b)
{
	assert(b != NULL);
	return b->bitcount;
}

void *bitset_data(bitset b)
{
	assert(b != NULL);
	return b->data;
}

void bitset_clear(bitset b, size_t i)
{
	assert(b != NULL);
	assert(i < b->size * CHAR_BIT);

	b->data[i / CHAR_BIT] &= ~(1 << (i % CHAR_BIT));
}

void bitset_remap(bitset b, void *mem, size_t cb)
{
	assert(b != NULL);
	assert(mem != NULL);
	assert(cb > 0);

	free(b->data);
	b->data = mem;
	b->size = cb;
}

bitset bitset_and(bitset b, bitset c)
{
	size_t i, nbits, size;
	bitset a;
	unsigned char *sa, *sb, *sc;

	assert(b != NULL);
	assert(c != NULL);
	assert(bitset_size(b) == bitset_size(c));

	nbits = bitset_bitcount(b);
	if ((a = bitset_new(nbits)) == NULL)
		return NULL;

	sa = bitset_data(a);
	sb = bitset_data(b);
	sc = bitset_data(c);

	size = bitset_size(b);
	for (i = 0; i < size; i++)
		*sa++ = *sb++ & *sc++;

	return a;
}

bitset bitset_or(bitset b, bitset c)
{
	size_t i, nbits, size;
	bitset a;
	unsigned char *sa, *sb, *sc;

	assert(b != NULL);
	assert(c != NULL);
	assert(bitset_size(b) == bitset_size(c));

	nbits = bitset_bitcount(b);
	if ((a = bitset_new(nbits)) == NULL)
		return NULL;

	sa = bitset_data(a);
	sb = bitset_data(b);
	sc = bitset_data(c);

	size = bitset_size(b);
	for (i = 0; i < size; i++)
		*sa++ = *sb++ | *sc++;

	return a;
}

bitset bitset_xor(bitset b, bitset c)
{
	size_t i, nbits, size;
	bitset a;
	unsigned char *sa, *sb, *sc;

	assert(b != NULL);
	assert(c != NULL);
	assert(bitset_size(b) == bitset_size(c));

	nbits = bitset_bitcount(b);
	if ((a = bitset_new(nbits)) == NULL)
		return NULL;

	sa = bitset_data(a);
	sb = bitset_data(b);
	sc = bitset_data(c);

	size = bitset_size(b);
	for (i = 0; i < size; i++)
		*sa++ = *sb++ ^ *sc++;

	return a;
}

void bitset_and_eq(bitset a, bitset b)
{
	size_t i;

	assert(a != NULL);
	assert(b != NULL);
	assert(bitset_size(a) == bitset_size(b));

	for (i = 0; i < a->size; i++)
		a->data[i] &= b->data[i];
}

void bitset_or_eq(bitset a, bitset b)
{
	size_t i;

	assert(a != NULL);
	assert(b != NULL);
	assert(bitset_size(a) == bitset_size(b));

	for (i = 0; i < a->size; i++)
		a->data[i] |= b->data[i];
}

void bitset_xor_eq(bitset a, bitset b)
{
	size_t i;

	assert(a != NULL);
	assert(b != NULL);
	assert(bitset_size(a) == bitset_size(b));

	for (i = 0; i < a->size; i++)
		a->data[i] ^= b->data[i];
}

int bitset_cmp(const bitset a, const bitset b)
{
	size_t i;

	assert(a != NULL);
	assert(b != NULL);
	assert(bitset_size(a) == bitset_size(b));

	for (i = 0; i < a->size; i++) {
		int delta = a->data[i] - b->data[i];
		if (delta)
			return delta;
	}

	return 0;
}

#ifdef BITSET_CHECK

#include <stdio.h>

int main(void)
{
	bitset a, b, c;
	size_t i, nelem = 10000;

	if ((b = bitset_new(nelem)) == NULL)
		return 1;

	for (i = 0; i < nelem; i++)
		bitset_set(b, i);

	for (i = 0; i < nelem; i++)
		if (!bitset_is_set(b, i))
			return 1;

	for (i = 0; i < nelem; i++)
		bitset_clear(b, i);

	for (i = 0; i < nelem; i++)
		if (bitset_is_set(b, i))
			return 1;

	// Now test the binary ops.
	if ((c = bitset_dup(b)) == NULL)
		return 1;

	// Since both b and c are all-zero, b & c == b | c == all-zero
	// It's an error if a has 1..n bit high.
	if ((a = bitset_and(b, c)) == NULL)
		return 1;

	if (!bitset_allzero(a))
		return 1;

	bitset_free(a);

	// Now do the same for _or(). 
	if ((a = bitset_or(b, c)) == NULL)
		return 1;

	if (!bitset_allzero(a))
		return 1;

	bitset_free(a);

	// Now do the same for _xor().
	if ((a = bitset_xor(b, c)) == NULL)
		return 1;

	if (!bitset_allzero(a))
		return 1;

	bitset_free(a);

	// Now set every second bit in b and c. Even bits in b and odd
	// bits in c. Then repeat all the tests.
	for (i = 0; i < nelem; i++) {
		if (i % 2 == 0)
			bitset_set(b, i);
		else
			bitset_set(c, i);
	}

	// Since no bits in b and c match, a should be all zero.
	if ((a = bitset_and(b, c)) == NULL)
		return 1;

	if (!bitset_allzero(a))
		return 1;

	bitset_free(a);

	// All bits should be high, since b and c has the patterns 101010 and 010101
	if ((a = bitset_or(b, c)) == NULL)
		return 1;

	if (!bitset_allone(a))
		return 1;

	bitset_free(a);

	// Now do the same for _xor(). 1 ^ 0 == 1.
	if ((a = bitset_xor(b, c)) == NULL)
		return 1;

	if (!bitset_allone(a))
		return 1;

	bitset_free(a);

	// b and c still has the 1010 and 0101 patterns. Let's use
	// them to check the new _eq() functions.
	bitset_and_eq(b, c); // b Now is all zero, hopefully
	if (!bitset_allzero(b))
		return 1;

	// If we now OR b and c, then b should equal c.
	bitset_or_eq(b, c);
	if (bitset_allzero(b) || bitset_allone(b))
		return 1;
	if (bitset_cmp(b, c) != 0)
		return 1;

	// Right, if we now XOR b and c, then b will be all zero.
	bitset_xor_eq(b, c);
	if (!bitset_allzero(b))
		return 1;

	// Now test the binary ops.
	if ((c = bitset_dup(b)) == NULL)
		return 77;

	// Since both b and c are all-zero, b & c == b | c == all-zero
	// It's an error if a has 1..n bit high.
	if ((a = bitset_and(b, c)) == NULL)
		return 77;

	if (!bitset_allzero(a))
		return 77;

	bitset_free(a);

	// Now do the same for _or(). 
	if ((a = bitset_or(b, c)) == NULL)
		return 77;

	if (!bitset_allzero(a))
		return 77;

	bitset_free(a);

	// Now do the same for _xor().
	if ((a = bitset_xor(b, c)) == NULL)
		return 77;

	if (!bitset_allzero(a))
		return 77;

	bitset_free(a);

	// Now set every second bit in b and c. Even bits in b and odd
	// bits in c. Then repeat all the tests.
	for (i = 0; i < nelem; i++) {
		if (i % 2 == 0)
			bitset_set(b, i);
		else
			bitset_set(c, i);
	}

	// Since no bits in b and c match, a should be all zero.
	if ((a = bitset_and(b, c)) == NULL)
		return 77;

	if (!bitset_allzero(a))
		return 77;

	bitset_free(a);

	// All bits should be high, since b and c has the patterns 101010 and 010101
	if ((a = bitset_or(b, c)) == NULL)
		return 77;

	if (!bitset_allone(a))
		return 77;

	bitset_free(a);

	// Now do the same for _xor(). 1 ^ 0 == 1.
	if ((a = bitset_xor(b, c)) == NULL)
		return 77;

	if (!bitset_allone(a))
		return 77;

	bitset_free(a);

	// b and c still has the 1010 and 0101 patterns. Let's use
	// them to check the new _eq() functions.
	bitset_and_eq(b, c); // b Now is all zero, hopefully
	if (!bitset_allzero(b))
		return 77;

	// If we now OR b and c, then b should equal c.
	bitset_or_eq(b, c);
	if (bitset_allzero(b) || bitset_allone(b))
		return 77;
	if (bitset_cmp(b, c) != 0)
		return 77;

	// Right, if we now XOR b and c, then b will be all zero.
	bitset_xor_eq(b, c);
	if (!bitset_allzero(b))
		return 77;

	bitset_free(b);
	bitset_free(c);
	return 0;
}

#endif

