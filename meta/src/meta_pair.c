/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_pair.h>

pair pair_new(size_t nelem)
{
    pair new;

    assert(nelem > 0);

    if ((new = malloc(sizeof *new)) == NULL
    || (new->element = calloc(nelem, sizeof *new->element)) == NULL) {
        free(new);
        return NULL;
    }

    new->used = 0;
    new->nelem = nelem;

    return new;
}

void pair_free(pair this)
{
    size_t i;

    if (this == NULL)
        return;

    for (i = 0; i < this->used; i++) {
        free(this->element[i].name);
        free(this->element[i].value);
    }

    free(this->element);
    free(this);
}

static status_t pair_extend(pair this, size_t addcount)
{
    struct pair_element* new;

    assert(this != NULL);

    new = realloc(this->element, sizeof *this->element * (this->nelem + addcount));
    if (new == NULL)
        return failure;

    this->element = new;
    this->nelem += addcount;
    return success;
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
    struct pair_element* new; /* Just a helper to beautify the code */
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
