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

#ifndef META_LIST_H
#define META_LIST_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <assert.h>

/* Declaration of our list ADT */
typedef struct list_tag* list;

/*
 * The list is implemented as a double linked list.
 * The first node in the list never contains data.
 * This makes it easy to insert at front.
 *
 * As a little trick, the data member in the first node
 * points to the last node. This makes it a lot faster
 * to add new nodes.
 */
struct list_tag {
	struct list_tag *next, *prev;
	void* data;
};

/* Our list iterator.  */
typedef struct list_iterator_tag {
	void *node;
} list_iterator;

/*
 * This function type is used by the functions
 * list_foreach, list_foreach_reversed and 
 * list_foreach_sep.
 */
typedef int (*listfunc)(void* arg, void*data);

list list_new(void);
void list_free(list lst, dtor free_fn);

list list_copy(list lst, void*(*copier)(const void*), dtor dtor_fn);
list list_add(list lst, void *data);
list list_insert(list lst, void *data);
int list_insert_before(list_iterator li, void *data);
int list_insert_after(list_iterator li, void *data);
size_t list_size(list lst);

list_iterator list_delete(list lst, list_iterator i, dtor dtor_fn);
list_iterator list_remove_node(list lst, list_iterator i);
list_iterator list_find(list lst, const void *data, int(*compar)(const void*, const void*));


static inline list_iterator list_first(list lst)
{
	list_iterator i;

	assert(lst != NULL);
	i.node = lst->next;
	return i;
}

static inline int list_end(list_iterator li)
{
	return li.node == NULL;
}

static inline void* list_get(list_iterator i)
{
	assert(i.node != NULL);
	return ((list)i.node)->data;
}

static inline list_iterator list_next(list_iterator i)
{
	i.node = ((list)i.node)->next;
	return i;
}

int list_last(list_iterator li);

void* list_get_item(list lst, size_t idx);

int list_foreach(list lst, void* args, listfunc f);
int list_foreach_reversed(list lst, void* arg, listfunc f);
int list_foreach_sep(list lst, void* arg, listfunc f, int(*sep)(void*arg));
int list_dual_foreach(list lst, void *arg1, void *arg2, int(*dual)(void* a1, void *a2, void *data));

list list_merge(list dest, list src);
void list_sort(list lst, int(*func)(const void *p1, const void *p2)); 
size_t list_count(list lst, int (*include_node)(void*));

/* Sublists */
list sublist_copy(list lst);
list sublist_adaptor(list lst, void* (*adaptor)(void*));
list sublist_create(list lst, int (*include_node)(void*));
list sublist_create_neg(list lst, int (*include_node)(void*));

void sublist_free(list lst);

#ifdef __cplusplus
}
#endif

#endif  /* guard */

