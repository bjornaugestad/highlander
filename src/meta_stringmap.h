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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef META_STRINGMAP_H
#define META_STRINGMAP_H

#include <meta_list.h>

/**
 * @brief maps a string to a unique integer value.
 * This ADT is thread safe and reentrant, but please
 * remember that you need to serialize access if you call multiple
 * functions. The typical example is:
 * @code
 * if(!stringmap_exists(...))
 *    stringmap_add(...);
 * @endcode
 * Such code is dangerous in MT programs unless you lock access to the stringmap,
 * since another thread may add the same value inbetween the call
 * to the stringmap_exists() function and the stringmap_add() function.
 */

typedef struct stringmap_tag* stringmap;

stringmap stringmap_new(size_t nelem);
void stringmap_free(stringmap sm);

/** Adds a new item to the stringmap */
int stringmap_add(stringmap sm, const char* s, unsigned long* pid);

/** Returns 1 if the string exists, else 0 */
int stringmap_exists(stringmap sm, const char* s);

/**
 * Remove all entries from the stringmap, the stringmap itself is reusable.
 * Good to have if you want to refresh the cache.
 */
int stringmap_invalidate(stringmap sm);

/** Get id for a given string. Returns 1 if the string exists, else 0 */
int stringmap_get_id(stringmap sm, const char* s, unsigned long* pid);

/*
 * Walk the stringmap, calling the callback function once for
 * each element in the map. The callback must return 1 to continue
 * or 0 to stop walking.
 * The callback function will be called with the mapped value s and the user
 * proviced argument arg, which can be NULL.
 */
int stringmap_foreach(stringmap sm, int(*fn)(const char*s, void* arg), void* arg);

/*
 * Return a stringmap containing elements present in sm1 only.
 * If sm1 contains A,B,C,D and sm2 contains A,C,E,F then the
 * returned stringmap will contain B,D.
 *
 * Remember to free it after use.
 */
stringmap stringmap_subset(stringmap sm1, stringmap sm2);

/* Convert a stringmap to a list. Strings will be copied  */
list stringmap_tolist(stringmap sm);


#endif

