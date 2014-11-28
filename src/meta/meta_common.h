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

#ifndef META_COMMON_H
#define META_COMMON_H


#ifdef __cplusplus
extern "C" {
#endif

/* stuff common to all modules.  */

/*
 * A destructor (dtor) is a pointer to a function which frees
 * an object and all memory allocated by that object. We use dtor's
 * quite a lot, e.g. to destroy list members, array members and pool members.
 */
typedef void(*dtor)(void*);

/* We always need to be able to trace stuff, mainly for debugging.
 * Even if we think that everything is working fine, shit happens.
 * We therefore trace all functions called, including tid (if any)
 * We always support the following global variables:
 * int meta_verbose_level. 0 means no output
 * int meta_indent_level. For output
 * These variables are always present so that command line processing
 * can be simpler. They just don't do anything in release builds (NDEBUG).
 */
extern int meta_verbose_level;
extern int meta_indent_level;

void verbose(int level, const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif /* guard */