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

#ifndef META_SLOTBUF_H
#define META_SLOTBUF_H

#include <stddef.h> /* for size_t */

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief A slotbuf is a buffer of void* pointers.
 *
 * The special thing is that the pointers can be indexed by any
 * value, this adt will modify the index to fit within the size
 * of the buffer.
 *
 * So what's the purpose of this adt? The idea is to have a general
 * buffer/array with a fixed size and then be able to address
 * entries in the buffer with a steadily increasing index, e.g. time
 * or some counter without having to adjust for offsets.
 * The set/get functions will compute the correct index like this:
 *		actual_index = index % bufsize;
 *
 * The buffer comes in two flavors, one will allow you to overwrite
 * entries without warnings and the other will not.
 *
 * The buffer is thread safe and we use external locking.
 */

typedef struct slotbuf_tag *slotbuf;

/*
 * Creates a new slotbuf. If can_overwrite == 0, then we cannot
 * assign new values to a slot that hasn't been read.
 */
slotbuf slotbuf_new(size_t size, int can_overwrite, dtor fn);

/*
 * Frees  the slotbuf and slot entries if a destructor is provided.
 */
void slotbuf_free(slotbuf p);

/*
 * Assigns a new value to slot i. Returns 1 on succes and 0 if a slot
 * had a value and can_overwrite is 0.
 */
int slotbuf_set(slotbuf p, size_t i, void* value);

/*
 * Returns the data in slot i, if any. Will clear the slot.
 */
void* slotbuf_get(slotbuf p, size_t i);

/*
 * Returns the data in slot i, if any. Does not clear the slot.
 */
void* slotbuf_peek(slotbuf p, size_t i);

/*
 * Returns 1 if data exists in the slot, else 0
 */
int slotbuf_has_data(slotbuf p, size_t i);

/*
 * Returns the number of elements in the slotbuf
 */
size_t slotbuf_nelem(slotbuf p);

void slotbuf_lock(slotbuf p);
void slotbuf_unlock(slotbuf p);

#ifdef __cplusplus
}
#endif

#endif
