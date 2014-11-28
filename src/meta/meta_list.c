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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>

#include <meta_list.h>

list list_new(void)
{
	list lst;

	if ((lst = malloc(sizeof *lst)) != NULL) {
		lst->next = NULL;
		lst->prev = NULL;
		lst->data = NULL;
	}

	return lst;
}

void list_free(list lst, void(*cleanup)(void*))
{
	list p;

	if (lst != NULL) {
		/* Free data for all items except the first. */
		for (p = lst->next; p != NULL; p = p->next) {
			if (cleanup != NULL)
				cleanup(p->data);
			else
				free(p->data);
		}

		/* Free the list itself */
		while (lst) {
			p = lst;
			lst = lst->next;
			free(p);
		}
	}
}

list list_add(list lst, void *data)
{
	list node;
	int err = 0;
	int we_allocated_list = 0;

	/* Allocate new list if first param is NULL */
	if (lst == NULL) {
		if ((lst = list_new()) == NULL)
			return NULL;
		else
			we_allocated_list = 1;
	}

	if ((node = calloc(1, sizeof *node)) == NULL) {
		if (we_allocated_list)
			list_free(lst, NULL);
		return NULL;
	}
	else {
		if (lst->data != NULL) {
			/* We have a cached end */
			node->prev = lst->data;
			node->prev->next = node;
		}
		else {
			/* This is the first item we add */
			lst->next = node;
			node->prev = lst;
		}

		node->data = data;

		/* Cache the new item */
		lst->data = node;
	}

	return err ? NULL : lst;
}

/*
 * Make the next node the current so that list_next() loops will
 * work as expected. By expected I mean that if you iterate on a 3-node list,
 * (A,B,C), and then delete B, list_next() returns C.
 */
list_iterator list_remove_node(list lst, list_iterator old)
{
	list_iterator new;
	list node = old.node;

	assert(lst != NULL);

	/* Update cache */
	if (lst->data == node)
		lst->data = node->prev;

	/* Remove node from backward pointing list */
	if (node->next != NULL)
		node->next->prev = node->prev;

	/* Now remove this node in the forward pointing list */
	node->prev->next = node->next;

	/* 'Create' the new iterator */
	new.node = node->next;

	/* Free memory used by the node, but do not free
	 * memory used by the data. This is intended.
	 */
	free(node);
	return new;
}

list_iterator list_delete(list lst, list_iterator i, void(*cleanup)(void*))
{
	list node = i.node;

	assert(lst != NULL);

	if (cleanup != NULL)
		cleanup(node->data);
	else
		free(node->data);

	return list_remove_node(lst, i);
}


list_iterator list_find(
	list lst,
	const void *data,
	int(*cmp)(const void*, const void*))
{
	list_iterator i;

	i.node = NULL;
	for (lst = lst->next; lst != NULL; lst = lst->next) {
		if (cmp(data, lst->data) == 0) {
			i.node = lst;
			break;
		}
	}

	return i;
}

int list_foreach(list lst, void* args, int(*f)(void* args, void* data))
{
	list node;

	assert(lst != NULL);
	assert(f != NULL);

	for (node = lst->next; node != NULL; node = node->next) {
		if (f(args, node->data) == 0)
			return 0;
	}

	return 1;
}

int list_dual_foreach(
	list lst,
	void *arg1,
	void *arg2,
	int(*dual)(void* a1, void *a2, void *data))
{
	list node;
	if (lst->next != NULL) {
		for (node = lst->next; node != NULL; node = node->next) {
			if (dual(arg1, arg2, node->data) == 0)
				return 0;
		}
	}

	return 1;
}

int list_foreach_reversed(
	list lst,
	void* args,
	int(*f)(void* args, void* data))
{
	list node;

	/* Don't iterate on empty lists */
	if (lst->data != NULL) {
		for (node = lst->data; node != lst ; node = node->prev) {
			if (f(args, node->data) == 0)
				return 0;
		}
	}

	return 1;
}

size_t list_size(list lst)
{
	size_t size = 0;
	for (lst = lst->next; lst != NULL; lst = lst->next)
		size++;

	return size;
}

list sublist_create(list lst, int(*include_node)(void*))
{
	list sublist, node;

	assert(lst != NULL);
	assert(include_node != NULL);

	if ((sublist = list_new()) == NULL)
		return NULL;

	for (node = lst->next; node != NULL; node = node->next) {
		if (include_node(node->data)) {
			if (list_add(sublist, node->data) == NULL) {
				sublist_free(sublist);
				return NULL;
			}
		}
	}

	return sublist;
}

list sublist_create_neg(list lst, int(*include_node)(void*))
{
	list node, sublist;

	assert(lst != NULL);
	assert(include_node != NULL);

	if ((sublist = list_new()) == NULL)
		return NULL;

	for (node = lst->next; node != NULL; node = node->next) {
		if (!include_node(node->data)) {
			if (list_add(sublist, node->data) == NULL) {
				sublist_free(sublist);
				return NULL;
			}
		}
	}

	return sublist;
}

/* Free the list only, not the data. */
void sublist_free(list lst)
{
	list p;

	while (lst != NULL) {
		p = lst;
		lst = lst->next;
		free(p);
	}
}

list list_merge(list dest, list src)
{
	list_iterator i;
	void* data;

	assert(src != NULL);

	for (i = list_first(src); !list_end(i); i = list_next(i)) {
		data = list_get(i);
		if ((dest = list_add(dest, data)) == NULL)
			return NULL;
	}

	sublist_free(src);
	return dest;
}


int list_foreach_sep(
	list lst,
	void* args,
	int(*f)(void* arg, void* data),
	int(*sep)(void*arg))
{
	list node;

	for (node = lst->next; node != NULL; node = node->next) {
		if (f(args, node->data) == 0)
			return 0;

		/* Call sep if more items in the list */
		if (node->next != NULL) {
			if (sep(args) == 0)
				return 0;
		}
	}

	return 1;
}

list sublist_adaptor(list src, void* (*adaptor)(void*))
{
	list dest = list_new();
	if (dest != NULL) {
		for (src = src->next; src != NULL; src = src->next) {
			if (list_add(dest, adaptor(src->data)) == NULL) {
				/*
				 * We cannot free the list since we have no
				 * idea of what the adaptor has done. Clean up as
				 * much as possible by freeing the nodes, but leave
				 * the data untouched.
				 */
				sublist_free(dest);
				return NULL;
			}
		}
	}

	return dest;
}


list sublist_copy(list src)
{
	list dest;

	if ((dest = list_new()) == NULL)
		return NULL;

	for (src = src->next; src != NULL; src = src->next) {
		if (list_add(dest, src->data) == NULL) {
			sublist_free(dest);
			return NULL;
		}
	}

	return dest;
}

list list_insert(list lst, void *data)
{
	int we_allocated_list = 0;
	if (lst == NULL) {
		lst = list_new();
		we_allocated_list = 1;
	}

	if (lst != NULL) {
		list node;

		if ((node = calloc(1, sizeof *node)) == NULL) {
			if (we_allocated_list)
				list_free(lst, NULL);

			return NULL;
		}

		node->data = data;
		node->next = lst->next;
		node->prev = lst;
		if (lst->next)
			lst->next->prev = node;

		lst->next = node;
		if (lst->data == NULL)
			lst->data = node;
	}

	return lst;
}

int list_insert_before(list_iterator li, void *data)
{
	list new, curr;

	assert(li.node != NULL);
	assert(data != NULL);

	if ((new = calloc(1, sizeof *new)) == NULL)
		return ENOMEM;

	curr = li.node;
	new->data = data;
	new->next = curr;
	new->prev = curr->prev;
	curr->prev->next = new;
	curr->prev = new;
	return 0;
}

int list_insert_after(list_iterator li, void *data)
{
	list new, curr;

	assert(li.node != NULL);
	assert(data != NULL);

	if ((new = calloc(1, sizeof *new)) == NULL)
		return ENOMEM;

	curr = li.node;
	new->data = data;
	new->prev = curr;
	new->next = curr->next;
	if (curr->next != NULL)
		curr->next->prev = new;

	curr->next = new;
	return 0;
}

void list_sort(list lst, int(*func)(const void *p1, const void *p2))
{
	int i;
	list s;
	int sorted = 0;
	void *p1, *p2;

	assert(lst != NULL);
	assert(func != NULL);

	while (!sorted) {
		sorted = 1;
		for (s = lst->next; s != NULL; s = s->next) {
			p1 = s->data;
			if (s->next == NULL)
				break;

			p2 = s->next->data;
			i = func(p1, p2);
			if (i > 0) {
				s->next->data = p1;
				s->data = p2;
				sorted = 0;
			}
		}
	}
}

size_t list_count(list lst, int (*include_node)(void*))
{
	size_t count = 0;
	list node;

	if (lst->next == NULL)
		return count;

	for (node = lst->next; node != NULL; node = node->next) {
		if (include_node == NULL || include_node(node->data))
			count++;
	}

	return count;
}

int list_last(list_iterator li)
{
	li = list_next(li);
	if (list_end(li))
		return 1;
	else
		return 0;
}

list list_copy(list lst, void*(*copier)(const void*), dtor dtor_fn)
{
	list node = NULL, new = NULL;

	assert(lst != NULL);
	assert(copier != NULL);

	if ((new = list_new()) != NULL) {
		for (node = lst->next; node != NULL; node = node->next) {
			void *new_data;

			if ((new_data = copier(node->data)) == NULL) {
				list_free(new, dtor_fn);
				return NULL;
			}
			else if(list_add(new, new_data) == NULL) {
				list_free(new, dtor_fn);
				return NULL;
			}
		}
	}

	return new;
}

void* list_get_item(list lst, size_t index)
{
	list node;

	assert(lst != NULL);
	assert(index < list_size(lst));

	for (node = lst->next; node != NULL; node = node->next) {
		if (index-- == 0)
			return node->data;
	}

	return NULL;
}

#ifdef LIST_CHECK

/* Some struct to put in the list. */
struct item {
	size_t value;
};

static void item_dtor(void *p)
{
	free(p);
}

static int item_cmp(const void* p1, const void* p2)
{
	const struct item *i1 = p1, *i2 = p2;

	return i1->value - i2->value;
}

static void* item_dup(const void* arg)
{
	const struct item* src = arg;
	struct item* p;

	if ((p = malloc(sizeof *p)) != NULL)
		*p = *src;

	return p;
}

static int item_bottom_half(void* arg)
{
	struct item* p = arg;
	return p->value < 500;
}

static void* item_adapt_value(struct item* p)
{
	assert(p != NULL);
	return &p->value;
}

static int item_foreach(void* arg, void* item)
{
	assert(arg == NULL);
	assert(item != NULL);
	(void)arg;
	(void)item;

	return 1;
}

static int item_foreach2(void* arg1, void* arg2, void* item)
{
	assert(arg1 == NULL);
	assert(arg2 == NULL);
	assert(item != NULL);

	(void)arg1;
	(void)arg2;
	(void)item;

	return 1;
}

static int item_sep(void* arg)
{
	assert(arg == NULL);
	(void)arg;

	return 1;
}

static void return77(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(77);
}

int main(void)
{
	list a, b;
	list_iterator li;
	size_t i, nelem = 1000;
	struct item *node, searchterm;
	const char *s;

#if 1
	if ((a = list_new()) == NULL)
		return77("Could not allocate memory");

	/* Add items to the list */
	for (i = 0; i < nelem; i++)	 {
		if ((node = malloc(sizeof *node)) == NULL)
			return77("Could not create nodes");

		node->value = i;
		if (list_add(a, node) == NULL)
			return77("Could not add nodes to list");
	}

	if (list_size(a) != nelem)
		return77("Incorrect number of nodes in list");

	/* Now check that the same items are in the list */
	i = 0;
	for (li = list_first(a); !list_end(li); li = list_next(li)) {
		if ((node = list_get(li)) == NULL)
			return77("Could not get node from list");

		if (node->value != i)
			return77("Incorrect value for node");

		i++;
	}

	/* Search for an existing list item */
	searchterm.value = nelem - 1;
	li = list_find(a, &searchterm, item_cmp);
	if (list_end(li))
		return77("Could not locate valid node");

	/* Search for a non-existing list item */
	searchterm.value = 0xdeadbeef;
	li = list_find(a, &searchterm, item_cmp);
	if (!list_end(li))
		return77("Found invalid node");

	/* Copy the list */
	if ((b = list_copy(a, item_dup, item_dtor)) == NULL)
		return77("Could not copy list");

	list_free(b, item_dtor);

	/* Create a sublist containing just some items */
	if ((b = sublist_create(a, item_bottom_half)) == NULL)
		return77("Could not create sublist");

	/* Check that all items are below 500 */
	for (li = list_first(b); !list_end(li); li = list_next(li)) {
		if ((node = list_get(li)) == NULL)
			return77("Could not get node from list");
		else if(node->value >= 500)
			return77("Incorrect value for node");
	}
	sublist_free(b);

	/* Now create the inverse of that list, using the same item_bottom_half() function */
	if ((b = sublist_create_neg(a, item_bottom_half)) == NULL)
		return77("Could not create negated list");

	/* Check that all items are >= 500 */
	for (li = list_first(b); !list_end(li); li = list_next(li)) {
		if ((node = list_get(li)) == NULL)
			return77("Could not get node from list");
		else if(node->value < 500)
			return77("Incorrect value, should be >= 500");
	}
	sublist_free(b);

	/* Now adapt the original list to a list pointing directly to the integer member
	 * named value. Then check that we have the values we expect(0..999).
	 */
	if ((b = sublist_adaptor(a, (void*(*)(void*))item_adapt_value)) == NULL)
		return77("Could not adapt list");

	i = 0;
	for (li = list_first(b); !list_end(li); li = list_next(li)) {
		size_t* pi;
		if ((pi = list_get(li)) == NULL)
			return77("Could not get node from list");
		else if(*pi != i)
			return77("Incorrect value for adapted list");
		i++;
	}
	sublist_free(b);

	/* Insert an item in front of other items, then retrieve it and check that
	 * we got the right item back.
	 */
	if ((node = malloc(sizeof *node)) == NULL)
		return77("Could not alloc mem");
	node->value = 0xbeef;
	if (list_insert(a, node) == NULL)
		return77("Could not insert node");

	li = list_first(a);
	node = list_get(li);
	if (node->value != 0xbeef)
		return77("Incorrect value for inserted node");
	list_delete(a, li, item_dtor);

	/* Now we get items using a zero based index */
	for (i = 0; i < nelem; i++) {
		if ((node = list_get_item(a, i)) == NULL)
			return77("Could not get node by index");
		else if(node->value != i)
			return77("Incorrect value for indexed node");
	}

	/* Do something for each item */
	if (!list_foreach(a, NULL, item_foreach))
		return77("list_foreach failed");

	/* Do the same thing, but in reverse order */
	if (!list_foreach_reversed(a, NULL, item_foreach))
		return77("list_foreach_reversed failed");

	/* Now foreach() with a call to a separator function called inbetween
	 * each node. */
	if (!list_foreach_sep(a, NULL, item_foreach, item_sep))
		return77("list_foreach_sep failed");

	/* Now foreach with two args instead of one */
	if (!list_dual_foreach(a, NULL, NULL, item_foreach2))
		return77("list_dual_foreach failed");

	/* Merge list a with a NULL list, creating list b. List a
	 * will be invalid adter the merge */
	if ((b = list_merge(NULL, a)) == NULL)
		return77("list_merge failed");

	/* We prefer to work with a as the main list */
	a = b;

	/* Now copy a to b, then merge a and b. */
	if ((b = list_copy(a, item_dup, item_dtor)) == NULL)
		return77("list_copy failed");
	else if((b = list_merge(b, a)) == NULL)
		return77("list_merge failed");
	else if(list_size(b) != nelem * 2)
		return77("Incorrect number of items in list");
	else {
		/* Remember that list_merge() freed list a */
		a = b;
	}

	list_sort(a, item_cmp);
	i = list_count(a, (int(*)(void*))item_bottom_half);
	list_free(a, item_dtor);
#endif

	a = list_new();

	/* Add items to the list. Then delete each of them using
	 * list_delete within a traditional for() loop. */
	for (i = 0; i < nelem; i++)	 {
		if ((node = malloc(sizeof *node)) == NULL)
			return77("Could not create nodes");

		node->value = i;
		if (list_add(a, node) == NULL)
			return77("Could not add nodes to list");
	}

	li = list_first(a);

	while (!list_end(li)) {
		struct item *p = list_get(li);
		fprintf(stderr, "%zu\n", p->value);
		li = list_delete(a, li, item_dtor);
	}

	list_free(a, item_dtor);

	/* Check list_insert_before. Should work for lists with 1..n items,
	 * but not 0 items since we cannot have a valid list iterator for
	 * such lists. */
	if ((a = list_new()) == NULL)
		return77("Could not allocate memory");

	list_add(a, strdup("foo"));
	if (list_size(a) != 1)
		return77("List has incorrect size. Expected 1, got %zu.", list_size(a));

	li = list_first(a);
	if (list_insert_before(li, strdup("bar")) != 0)
		return77("Could not add 'bar' before 'foo'");

	if (list_size(a) != 2)
		return77("List has incorrect size. Expected 2, got %zu.", list_size(a));

	// Check that the first item really is bar.
	li = list_first(a);
	s = list_get(li);
	if (strcmp(s, "bar") != 0)
		return77("Wrong value for item. Expected bar.");

	// Now insert baz after bar, so that the list will have three
	// items: bar, baz, foo
	if (list_insert_after(li, strdup("baz")))
		return77("Unable to add baz");

	if (list_size(a) != 3)
		return77("Expected 3 items in list. Got %zu", list_size(a));

	s = list_get_item(a, 0);
	if (strcmp(s, "bar") != 0)
		return77("Wrong value for item. Expected bar, got %s.", s);

	s = list_get_item(a, 1);
	if (strcmp(s, "baz") != 0)
		return77("Wrong value for item. Expected baz, got %s.", s);

	s = list_get_item(a, 2);
	if (strcmp(s, "foo") != 0)
		return77("Wrong value for item. Expected foo, got %s.", s);
	list_free(a, NULL);

	// Now test adding with one item in the list.
	a = list_new();
	list_add(a, strdup("foo"));
	list_insert_after(list_first(a), strdup("bar"));
	s = list_get_item(a, 1);
	if (strcmp(s, "bar") != 0)
		return77("Expected 'bar', got %s\n", s);

	list_free(a, NULL);
	return 0;
}

#endif
