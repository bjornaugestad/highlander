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

#ifndef META_CONFIGFILE_H
#define META_CONFIGFILE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct configfile_tag* configfile;

configfile configfile_read(const char *path);

bool configfile_exists(configfile cf, const char *name);

status_t configfile_get_string(configfile cf, const char *name,
	char *value, size_t cb) __attribute__((warn_unused_result));

status_t configfile_get_long(configfile cf, const char *name, long *value)
	__attribute__((warn_unused_result));

status_t configfile_get_ulong(configfile cf, const char *name,
	unsigned long *value)
	__attribute__((warn_unused_result));

status_t configfile_get_int(configfile cf, const char *name, int *value)
	__attribute__((warn_unused_result));

status_t configfile_get_uint(configfile cf, const char *name, 
	unsigned int *value) __attribute__((warn_unused_result));


void configfile_free(configfile cf);


#ifdef __cplusplus
}
#endif

#endif
