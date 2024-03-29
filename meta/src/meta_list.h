/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_LIST_H
#define META_LIST_H

#include <stdbool.h>
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
    void *data;
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
typedef bool (*listfunc)(void *arg, void*data);

list list_new(void)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void list_free(list lst, dtor free_fn);

list list_copy(list lst, void*(*copier)(const void*), dtor dtor_fn)
    __attribute__((warn_unused_result));

list list_add(list lst, void *data)
    __attribute__((warn_unused_result));

list list_insert(list lst, void *data)
    __attribute__((warn_unused_result));

status_t list_insert_before(list_iterator li, void *data)
    __attribute__((warn_unused_result));

status_t list_insert_after(list_iterator li, void *data)
    __attribute__((warn_unused_result));

size_t list_size(list lst)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

list_iterator list_delete(list lst, list_iterator i, dtor dtor_fn)
    __attribute__((nonnull(1)));

list_iterator list_remove_node(list lst, list_iterator i)
    __attribute__((nonnull));

list_iterator list_find(list lst, const void *data,
    int(*compar)(const void*, const void*))
    __attribute__((warn_unused_result))
    __attribute__((nonnull));


__attribute__((warn_unused_result))
static inline list_iterator list_first(list lst)
{
    list_iterator i;

    assert(lst != NULL);
    i.node = lst->next;
    return i;
}

__attribute__((warn_unused_result))
static inline int list_end(list_iterator li)
{
    return li.node == NULL;
}

__attribute__((warn_unused_result))
static inline void *list_get(list_iterator i)
{
    assert(i.node != NULL);
    return ((list)i.node)->data;
}

__attribute__((warn_unused_result))
static inline list_iterator list_next(list_iterator i)
{
    i.node = ((list)i.node)->next;
    return i;
}

bool list_last(list_iterator li)
    __attribute__((warn_unused_result));

void *list_get_item(list lst, size_t idx)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

bool list_foreach(list lst, void *args, listfunc f)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 3)));

bool list_foreach_reversed(list lst, void *arg, listfunc f)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 3)));

bool list_foreach_sep(list lst, void *arg, listfunc f, bool(*sep)(void*arg))
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 3, 4)));

bool list_dual_foreach(list lst, void *arg1, void *arg2,
    bool(*dual)(void *a1, void *a2, void *data))
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

// dest can be NULL. list_add() will create the list if needed.
list list_merge(list dest, list src)
    __attribute__((warn_unused_result));

void list_sort(list lst, int(*func)(const void *p1, const void *p2))
    __attribute__((nonnull));

size_t list_count(list lst, int (*include_node)(void*))
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

/* Sublists */
list sublist_copy(list lst)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

list sublist_adaptor(list lst, void *(*adaptor)(void*))
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

list sublist_create(list lst, int (*include_node)(void*))
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

list sublist_create_neg(list lst, int (*include_node)(void*))
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

void sublist_free(list lst);

#ifdef __cplusplus
}
#endif

#endif
