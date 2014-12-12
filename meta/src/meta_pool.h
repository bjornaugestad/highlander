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

#ifndef META_POOL_H
#define META_POOL_H

#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct pool_tag* pool;

pool pool_new(size_t nelem) __attribute__((malloc));
void pool_free(pool p, dtor cleanup);

void  pool_add(pool p, void *resource);
void *pool_get(pool p);
void  pool_recycle(pool p, void *resource);

#ifdef __cplusplus
}
#endif

#endif
