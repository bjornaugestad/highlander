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

#ifndef META_CACHE_H
#define META_CACHE_H

#include <stddef.h>
#include <meta_common.h>

typedef struct cache_tag* cache;

cache cache_new(size_t nelem, size_t hotlist_nelem, size_t cb)
	__attribute__((malloc));

void cache_free(cache c, dtor cleanup);

int cache_exists(cache c, size_t id)
	__attribute__((nonnull(1)));

status_t cache_add(cache c, size_t id, void *data, size_t cb, int pin)
	__attribute__((nonnull(1, 3)))
	__attribute__((warn_unused_result));

status_t cache_get(cache c, size_t id, void** pdata, size_t* pcb)
	__attribute__((nonnull(1, 3, 4)))
	__attribute__((warn_unused_result));

status_t cache_remove(cache c, size_t id)
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

#if 0
void cache_invalidate(cache c);
#endif


#endif
