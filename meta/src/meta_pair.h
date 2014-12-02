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

#ifndef PAIR_H
#define PAIR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * pair is used as a thread-safe way of storing multiple name/value pairs.
 * COMMENT: This is a map, not a "pair". The adt really needs to be renamed some day.
 */
typedef struct pair_tag* pair;
pair pair_new(size_t nelem);
void pair_free(pair p);

int	pair_add(pair p, const char *name, const char *value);
int	pair_set(pair p, const char *name, const char *value);
const char *pair_get_name(pair p, size_t i);
const char *pair_get(pair p, const char *name);

/* Sometimes we iterate on index to get names. Then there is no
 * need to get the value by name as we have the index anyway.
 * We avoid a lot of strcmps() this way.
 */
const char *pair_get_value_by_index(pair p, size_t i);
size_t pair_size(pair p);

#ifdef __cplusplus
}
#endif

#endif
