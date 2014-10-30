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

#ifndef META_STACK_H
#define META_STACK_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * file
 * @brief Implements a normal stack.
 */
typedef struct stack_tag* stack;

/*
 * Creates a new stack.
 */
stack stack_new(void);

/*
 * Frees an existing stack.
 * Items on the stack will not be freed.
 */
void stack_free(stack s);


/*
 * Pushes a new item onto the stack.
 * @param p Pointer to item to push. Cannot be NULL.
 * @return 1 if success, 0 if failure. Failure is ENOMEM.
 */
int stack_push(stack s, void *p);

/*
 * Returns the top item on the stack. Will return NULL if the
 * stack was empty. The debug version will abort() if the stack
 * was empty.
 */
void* stack_top(stack s);

/*
 * Removes the top item from the stack. This API
 * is inspired, IIRC, from the C++ STL stack implementation.
 */
void stack_pop(stack s);

/*
 * Returns the number of elements on the stack.
 */
size_t stack_nelem(stack s);

/*
 * We sometimes need to inspect the stack without popping the items
 * on it. This function returns the n'th element on the stack.
 * Note that element 0 is the top element and element n is the last(bottom)
 * element on the stack.
 * This function returns NULL if there are no item matching the index
 */
void* stack_get(stack s, size_t i);

#ifdef __cplusplus
}
#endif


#endif
