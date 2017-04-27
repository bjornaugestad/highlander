/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
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

#ifndef META_ARRAY_H
#define META_ARRAY_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct array_tag* array;

array array_new(size_t nmemb, int can_grow) 
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void  array_free(array a, dtor cln);

size_t array_nelem(array a)
    __attribute__((nonnull(1)));

void * array_get(array a, size_t ielem)
    __attribute__((nonnull(1)));

status_t array_add(array a, void *elem) 
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

status_t array_extend(array a, size_t nmemb) 
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
