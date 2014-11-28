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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

bitset bitset_new(size_t bitcount);
void bitset_free(bitset b);

void bitset_set(bitset b, size_t i);
void bitset_clear(bitset b, size_t i);
void bitset_clear_all(bitset b);
void bitset_set_all(bitset b);

int bitset_is_set(bitset b, size_t i);

size_t bitset_size(bitset b);

bitset bitset_map(void* mem, size_t cb);
void bitset_remap(bitset b, void* mem, size_t cb);
void bitset_unmap(bitset b);
void* bitset_data(bitset b);

#ifdef __cplusplus
}
#endif

#endif /* guard */
