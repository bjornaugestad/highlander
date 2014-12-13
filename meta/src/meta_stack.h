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

#ifndef META_STACK_H
#define META_STACK_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stack_tag* stack;

stack stack_new(void) 
	__attribute__((malloc));

void stack_free(stack s);

status_t stack_push(stack s, void *p) 
	__attribute__((nonnull(1)))
	__attribute__((warn_unused_result));

void *stack_top(stack s)
	__attribute__((nonnull(1)));

void stack_pop(stack s)
	__attribute__((nonnull(1)));

size_t stack_nelem(stack s)
	__attribute__((nonnull(1)));

void *stack_get(stack s, size_t i)
	__attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
