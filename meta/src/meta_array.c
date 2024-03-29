/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <assert.h>
#include <stdlib.h>

#include <meta_array.h>

array array_new(size_t nmemb, int can_grow)
{
    array p;

    assert(nmemb > 0);

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    if ((p->elements = calloc(nmemb, sizeof *p->elements)) == NULL) {
        free(p);
        return NULL;
    }

    p->can_grow = can_grow;
    p->nused = 0;
    p->nallocated = nmemb;

    return p;
}

void array_free(array a, dtor free_fn)
{
    if (a != NULL) {
        if (free_fn) {
            while (a->nused--)
                (*free_fn)(a->elements[a->nused]);
        }

        free(a->elements);
        free(a);
    }
}

status_t array_extend(array a, size_t nmemb)
{
    void *tmp;
    size_t n, size;

    assert(a != NULL);
    assert(nmemb > 0);

    n = a->nallocated + nmemb;
    size = sizeof *a->elements * n;

    if ((tmp = realloc(a->elements, size)) == NULL)
        return failure;

    a->elements = tmp;
    a->nallocated = n;
    return success;
}

#ifdef CHECK_ARRAY
#include <stdio.h>

int main(void)
{
    array a;
    size_t i, nelem = 10000;
    void *dummy;

    /* First a growable array */
    if ((a = array_new(nelem / 10, 1)) == NULL) {
        perror("array_new");
        return 1;
    }

    for (i = 0; i < nelem; i++) {
        dummy = (void*)(i+1);
        if (!array_add(a, dummy)) {
            perror("array_add");
            return 1;
        }
    }

    if (array_nelem(a) != nelem) {
        fprintf(stderr, "Mismatch between number of actual and expected items\n");
        return 1;
    }

    for (i = 0; i < nelem; i++) {
        if (array_get(a, i) == NULL) {
            fprintf(stderr, "Could not get array item %lu\n", (unsigned long)i);
            return 1;
        }
    }

    array_free(a, NULL);

    /* Now for a non-growable array */
    if ((a = array_new(nelem / 10, 0)) == NULL) {
        perror("array_new");
        return 1;
    }

    for (i = 0; i < nelem / 10; i++) {
        dummy = (void*)(i+1);
        if (!array_add(a, dummy)) {
            perror("array_add");
            return 1;
        }
    }

    /* Now we've filled all slots, the next call should fail */
    if (array_add(a, dummy) != 0) {
        fprintf(stderr, "Able to add to array which is full!\n");
        return 1;
    }

    if (array_nelem(a) != nelem / 10) {
        fprintf(stderr, "Mismatch between number of actual and expected items\n");
        return 1;
    }

    for (i = 0; i < nelem / 10; i++) {
        if (array_get(a, i) == NULL) {
            fprintf(stderr, "Could not get array item %lu\n", (unsigned long)i);
            return 1;
        }
    }

    array_free(a, NULL);
    return 0;
}
#endif
