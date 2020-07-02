/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_ARRAY_H
#define META_ARRAY_H

#include <assert.h>
#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct array_tag* array;

// Implementation of the array ADT.
struct array_tag {
    int can_grow;		/* Can the array grow automatically? */
    size_t nused;		/* How many that currently is in use */
    size_t nallocated;	/* How many that currently is allocated */
    void** elements;	/* Pointer to data */
};

array array_new(size_t nmemb, int can_grow)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void array_free(array a, dtor cln);

status_t array_extend(array a, size_t nmemb)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

__attribute__((warn_unused_result, nonnull))
static inline size_t array_nelem(array a)
{
    assert(a != NULL);
    return a->nused;
}

__attribute__((warn_unused_result, nonnull))
static inline void *array_get(array a, size_t ielem)
{
    assert(a != NULL);
    assert(ielem < array_nelem(a));

    if (ielem >= a->nused)
        return NULL;

    return a->elements[ielem];
}

__attribute__((warn_unused_result, nonnull))
static inline status_t array_add(array a, void *elem)
{
    assert(a != NULL);

    if (a->nused == a->nallocated) {
        if (!a->can_grow || !array_extend(a, a->nused))
            return failure;
    }

    a->elements[a->nused++] = elem;
    return success;
}

#ifdef __cplusplus
}
#endif

#endif
