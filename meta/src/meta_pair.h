/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_PAIR_H
#define META_PAIR_H

#include <stdbool.h>
#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

// pair is used as a thread-safe way of storing multiple name/value pairs.
// COMMENT: This is a map, not a "pair". The adt really needs
// to be renamed some day.
// Update 20170520: It still needs to be renamed! We already
// have another meta_map.h. Do we need both?
//
// meta_pair is used in one place only, the http_request adt
// to store params. Params in this context is request arguments,
// the ones starting with a ? and separated by ;.
// /foo.html?q=blah;z=flsdad;yet=another%20arg
// In that context, it's very useful to have this name/value map
// where both values are strings. the other map is a general
// purpose map which is way slower. Can we keep both, but maybe
// move the meta_pair from meta to http?
typedef struct pair_tag* pair;

// Local helper structure.
struct pair_element {
    char *name;
    char *value;
};

// Implementation of the meta_pair ADT
struct pair_tag {
    struct pair_element* element;
    size_t nelem;
    size_t used;
};

// Small helper returning the index to an element or 0 if not found
__attribute__((warn_unused_result, nonnull))
static inline bool pair_find(pair p, const char *name, size_t* pi)
{
    assert(p != NULL);
    assert(name != NULL);

    for (*pi = 0; *pi < p->used; (*pi)++) {
        if (0 == strcmp(p->element[*pi].name, name))
            return true;
    }

    return false;
}

__attribute__((warn_unused_result))
static inline size_t pair_size(pair this)
{
    assert(this != NULL);

    return this->used;
}

// Sometimes we iterate on index to get names. Then there is no
// need to get the value by name as we have the index anyway.
// We avoid a lot of strcmps() this way.
__attribute__((warn_unused_result))
static inline const char *pair_get_value_by_index(pair p, size_t i)
{
    assert(p != NULL);
    assert(i < p->nelem);

    return p->element[i].value;
}

/* Returns pointer to value if name exists, else NULL */
__attribute__((warn_unused_result))
static inline const char *pair_get(pair p, const char *name)
{
    size_t i;

    assert(p != NULL);
    assert(name != NULL);

    if (!pair_find(p, name, &i))
        return NULL;

    return p->element[i].value;
}

__attribute__((warn_unused_result))
static inline const char *pair_get_name(pair p, size_t idx)
{
    assert(p != NULL);
    assert(idx < p->used);

    return p->element[idx].name;
}


pair pair_new(size_t nelem)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

void pair_free(pair p);

status_t pair_add(pair p, const char *name, const char *value)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t pair_set(pair p, const char *name, const char *value)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));


#ifdef __cplusplus
}
#endif

#endif
