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

fifo fifo_new(size_t size) 
	__attribute__((warn_unused_result))
	__attribute__((malloc));

void fifo_free(fifo p, dtor dtor_fn);

status_t fifo_lock(fifo p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

status_t fifo_unlock(fifo p)
	__attribute__((nonnull(1)));

status_t fifo_add(fifo p, void *data) 
	__attribute__((nonnull(1, 2)))
	__attribute__((warn_unused_result));

size_t fifo_nelem(fifo p)
	__attribute__((nonnull(1)));

size_t fifo_free_slot_count(fifo p)
	__attribute__((nonnull(1)));

void *fifo_get(fifo p)
	__attribute__((nonnull(1)));

void *fifo_peek(fifo p, size_t i)
	__attribute__((nonnull(1)));

status_t fifo_write_signal(fifo p, void *data) 
	__attribute__((nonnull(1, 2)))
	__attribute__((warn_unused_result));

status_t fifo_wait_cond(fifo p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

status_t fifo_wake(fifo p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

status_t fifo_signal(fifo p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
