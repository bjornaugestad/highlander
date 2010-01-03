/*
    libcoremeta - A library of useful C functions and ADT's
    Copyright (C) 2000-2006 B. Augestad, bjorn.augestad@gmail.com 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

