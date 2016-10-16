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

#ifndef META_BITSET_H
#define META_BITSET_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bitset_tag* bitset;

bitset bitset_new(size_t bitcount) __attribute__((malloc));
bitset bitset_dup(bitset b) __attribute__((malloc));

void bitset_free(bitset b);

int bitset_cmp(const bitset a, const bitset b)
    __attribute__((nonnull(1, 2)));

void bitset_set(bitset b, size_t i)   __attribute__((nonnull(1)));
void bitset_set_all(bitset b)         __attribute__((nonnull(1)));
void bitset_clear(bitset b, size_t i) __attribute__((nonnull(1)));
void bitset_clear_all(bitset b)       __attribute__((nonnull(1)));

int bitset_is_set(const bitset b, size_t i) __attribute__((nonnull(1)));
int bitset_allzero(const bitset b)          __attribute__((nonnull(1)));
int bitset_allone(const bitset b)           __attribute__((nonnull(1)));

size_t bitset_size(bitset b)     __attribute__((nonnull(1)));
size_t bitset_bitcount(bitset b) __attribute__((nonnull(1)));

bitset bitset_map(void *mem, size_t cb) __attribute__((nonnull(1)));
void bitset_remap(bitset b, void *mem, size_t cb) __attribute__((nonnull(1, 2)));
void bitset_unmap(bitset b) __attribute__((nonnull(1)));
void *bitset_data(bitset b) __attribute__((nonnull(1)));

// equals a = b & c; and returns a. Remember to free it.
bitset bitset_and(bitset b1, bitset b2)
    __attribute__((nonnull(1, 2)))
    __attribute__((malloc));

bitset bitset_or(bitset b1, bitset b2)
    __attribute__((nonnull(1, 2)))
    __attribute__((malloc));

bitset bitset_xor(bitset b1, bitset b2)
    __attribute__((nonnull(1, 2)))
    __attribute__((malloc));

// equals a &= b;
void bitset_and_eq(bitset a, bitset b) __attribute__((nonnull(1, 2)));
void bitset_or_eq (bitset a, bitset b) __attribute__((nonnull(1, 2)));
void bitset_xor_eq(bitset a, bitset b) __attribute__((nonnull(1, 2)));

#ifdef __cplusplus
}
#endif

#endif
