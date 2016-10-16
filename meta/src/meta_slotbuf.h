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

#include <stdbool.h>
#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct slotbuf_tag *slotbuf;

slotbuf slotbuf_new(size_t size, int can_overwrite, dtor fn) 
    __attribute__((malloc));

void slotbuf_free(slotbuf p);

status_t slotbuf_set(slotbuf p, size_t i, void *value)
    __attribute__((nonnull(1, 3)))
    __attribute__((warn_unused_result));

void *slotbuf_get(slotbuf p, size_t i)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

void *slotbuf_peek(slotbuf p, size_t i)
    __attribute__((nonnull(1)));

bool slotbuf_has_data(slotbuf p, size_t i)
    __attribute__((nonnull(1)));

size_t slotbuf_nelem(slotbuf p)
    __attribute__((nonnull(1)));

void slotbuf_lock(slotbuf p)
    __attribute__((nonnull(1)));

void slotbuf_unlock(slotbuf p)
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
