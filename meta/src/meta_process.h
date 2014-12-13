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

#ifndef META_PROCESS_H
#define META_PROCESS_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct process_tag* process;

process process_new(const char *appname)
	__attribute__((malloc))
	__attribute__((warn_unused_result));

void process_free(process p);

int process_shutting_down(process p)
	__attribute__((nonnull(1)));

status_t process_set_rootdir(process p, const char *path)
	__attribute__((warn_unused_result))
	__attribute__((nonnull(1, 2)));

status_t process_set_username(process p, const char *username)
	__attribute__((warn_unused_result))
	__attribute__((nonnull(1, 2)));


status_t process_add_object_to_start(
	process p,
	void *object,
	status_t do_func(void *),
	status_t undo_func(void *),
	status_t run_func(void *),
	status_t shutdown_func(void *))
	__attribute__((warn_unused_result))
	__attribute__((nonnull(1, 2)));


status_t process_start(process p, int fork_and_close)
	__attribute__((warn_unused_result))
	__attribute__((nonnull(1)));

status_t process_wait_for_shutdown(process p)
	__attribute__((warn_unused_result))
	__attribute__((nonnull(1)));

int process_get_exitcode(process p, void *object)
	__attribute__((nonnull(1, 2)));


#ifdef __cplusplus
}
#endif

#endif
