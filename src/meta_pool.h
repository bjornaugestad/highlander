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
#ifndef META_POOL_H
#define META_POOL_H

#include <stddef.h> /* for size_t */

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct pool_tag* pool;

pool pool_new(size_t nelem);
void pool_free(pool p, dtor cleanup);

void  pool_add(pool p, void* resource);
void* pool_get(pool p);
void  pool_recycle(pool p, void* resource);

#ifdef __cplusplus
}
#endif

#endif /* guard */

