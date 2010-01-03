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
#ifndef META_CACHE_H
#define META_CACHE_H

#include <stddef.h>
#include <meta_common.h>

typedef struct cache_tag* cache;

cache cache_new(size_t nelem, size_t hotlist_nelem, size_t cb);
void cache_free(cache c, dtor cleanup);

int cache_add(cache c, size_t id, void* data, size_t cb, int pin);
int cache_exists(cache c, size_t id);
int cache_get(cache c, size_t id, void** pdata, size_t* pcb);
int cache_remove(cache c, size_t id);

#if 0
void cache_invalidate(cache c);
#endif


#endif

