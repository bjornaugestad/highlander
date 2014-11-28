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

#ifndef META_FIFO_H
#define META_FIFO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fifo_tag* fifo;

fifo fifo_new(size_t size);
void fifo_free(fifo p, dtor dtor_fn);

int fifo_lock(fifo p);
int fifo_unlock(fifo p);
int fifo_add(fifo p, void* data);

size_t fifo_nelem(fifo p);
size_t fifo_free_slot_count(fifo p);

void* fifo_get(fifo p);
void* fifo_peek(fifo p, size_t i);

int fifo_write_signal(fifo p, void* data);
int fifo_wait_cond(fifo p);
int fifo_wake(fifo p);
int fifo_signal(fifo p);

#ifdef __cplusplus
}
#endif

#endif
