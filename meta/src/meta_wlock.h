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

#ifndef META_WLOCK_H
#define META_WLOCK_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wlock_tag* wlock;

wlock wlock_new(void)
	__attribute__((malloc))
	__attribute__((warn_unused_result));

void wlock_free(wlock p);

status_t wlock_lock(wlock p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

status_t wlock_unlock(wlock p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

status_t wlock_signal(wlock p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

status_t wlock_wait(wlock p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

status_t wlock_broadcast(wlock p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
