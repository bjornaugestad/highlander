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
	size_t size;
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
		bitset_clear_all(b);
	}

	return b;
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

int bitset_is_set(bitset b, size_t i)
{
	assert(b != NULL);
	assert(i < b->size * CHAR_BIT);

	return b->data[i / CHAR_BIT] & (1 << (i % CHAR_BIT));
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

#ifdef BITSET_CHECK

int main(void)
{
	bitset b;
	size_t i, nelem = 10000;

	if ((b = bitset_new(nelem)) == NULL)
		return 77;

	for (i = 0; i < nelem; i++)
		bitset_set(b, i);

	for (i = 0; i < nelem; i++)
		if (!bitset_is_set(b, i))
			return 77;

	for (i = 0; i < nelem; i++)
		bitset_clear(b, i);

	for (i = 0; i < nelem; i++)
		if (bitset_is_set(b, i))
			return 77;

	bitset_free(b);
	return 0;
}

#endif

