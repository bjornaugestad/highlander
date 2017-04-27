/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
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
#include <string.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_pair.h>

/*
 * Local helper structure.
 */
struct element {
    char *name;
    char *value;
};

/*
 * Implementation of the meta_pair ADT
 */
struct pair_tag {
    struct element* element;
    size_t nelem;
    size_t used;
};

/* Small helper returning the index to an element or 0 if not found */
static int pair_find(pair p, const char *name, size_t* pi)
{
    assert(p != NULL);
    assert(name != NULL);

    for (*pi = 0; *pi < p->used; (*pi)++) {
        if (0 == strcmp(p->element[*pi].name, name))
            return 1;
    }

    return 0;
}

pair pair_new(size_t nelem)
{
    pair p;

    assert(nelem > 0);

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    if ((p->element = calloc(nelem, sizeof *p->element)) == NULL) {
        free(p);
        return NULL;
    }

    p->used = 0;
    p->nelem = nelem;

    return p;
}

void pair_free(pair p)
{
    size_t i;

    if (p == NULL)
        return;

    for (i = 0; i < p->used; i++) {
        free(p->element[i].name);
        free(p->element[i].value);
    }

    free(p->element);
    free(p);
}

static status_t pair_extend(pair p, size_t addcount)
{
    struct element* new;

    assert(p != NULL);

    new = realloc(p->element, sizeof *p->element * (p->nelem + addcount));
    if (new == NULL)
        return failure;

    p->element = new;
    p->nelem += addcount;
    return success;
}

size_t pair_size(pair p)
{
    assert(p != NULL);

    return p->used;
}

const char *pair_get_value_by_index(pair p, size_t i)
{
    assert(p != NULL);
    assert(i < p->nelem);

    return p->element[i].value;
}

/* Returns pointer to value if name exists, else NULL */
const char *pair_get(pair p, const char *name)
{
    size_t i;

    assert(p != NULL);
    assert(name != NULL);

    if (!pair_find(p, name, &i))
        return NULL;

    return p->element[i].value;
}


/* overwrites an existing value. Adds a new entry if needed.  */
status_t pair_set(pair p, const char *name, const char *value)
{
    size_t i, oldlen, newlen;

    assert(p != NULL);
    assert(name != NULL);

    if (!pair_find(p, name, &i))
        return pair_add(p, name, value);

    /* Modify existing value */
    oldlen = strlen(p->element[i].value);
    newlen = strlen(value);
    if (oldlen < newlen) {
        char *s = realloc(p->element[i].value, newlen + 1);
        if (NULL == s)
            return failure;

        p->element[i].value = s;
    }

    strcpy(p->element[i].value, value);
    return success;
}

status_t pair_add(pair p, const char *name, const char *value)
{
    struct element* new; /* Just a helper to beautify the code */
    size_t namelen, valuelen;

    assert(p != NULL);
    assert(name != NULL);

    /* Resize when needed */
    if (p->used == p->nelem && !pair_extend(p, p->nelem * 2))
        return failure;

    /* Assign the helper */
    new = &p->element[p->used];
    namelen = strlen(name) + 1;
    valuelen = strlen(value) + 1;
    if ((new->name = malloc(namelen)) == NULL)
        return failure;

    if ((new->value = malloc(valuelen)) == NULL) {
        free(new->name);
        return failure;
    }

    memcpy(new->name, name, namelen);
    memcpy(new->value, value, valuelen);
    p->used++;
    return success;
}

const char *pair_get_name(pair p, size_t idx)
{
    assert(p != NULL);
    assert(idx < p->used);

    return p->element[idx].name;
}

#ifdef CHECK_PAIR
#include <stdio.h>

int main(void)
{
    pair p;
    size_t i, j, niter = 100, nelem = 10;
    char name[1024], value[1024];
    const char *pval;

    for (i = 0; i < niter; i++) {
        if ((p = pair_new(nelem)) == NULL)
            return 1;

        /* Add a bunch of pairs */
        for (j = 0; j < nelem * 2; j++) {
            sprintf(name, "name %lu", (unsigned long)j);
            sprintf(value, "value %lu", (unsigned long)j);
            if (!pair_add(p, name, value))
                return 1;
        }

        /* Locate the same pairs and compare the returned value */
        for (j = 0; j < nelem * 2; j++) {
            sprintf(name, "name %lu", (unsigned long)j);
            sprintf(value, "value %lu", (unsigned long)j);
            if ((pval = pair_get(p, name)) == NULL)
                return 1;
            else if (strcmp(pval, value) != 0)
                return 1;
        }

        /* Manually extend the pair and then try again */
        if (!pair_extend(p, nelem * 2))
            return 1;

        /* Add a bunch of pairs */
        for (j = 0; j < nelem * 3; j++) {
            sprintf(name, "name %lu", (unsigned long)j);
            sprintf(value, "value %lu", (unsigned long)j);
            if (!pair_add(p, name, value))
                return 1;
        }

        /* Locate the same pairs and compare the returned value */
        for (j = 0; j < nelem * 3; j++) {
            sprintf(name, "name %lu", (unsigned long)j);
            sprintf(value, "value %lu", (unsigned long)j);
            if ((pval = pair_get(p, name)) == NULL)
                return 1;
            else if (strcmp(pval, value) != 0)
                return 1;
        }


        pair_free(p);

    }

    return 0;
}
#endif
