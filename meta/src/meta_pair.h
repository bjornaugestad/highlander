/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_PAIR_H
#define META_PAIR_H

#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * pair is used as a thread-safe way of storing multiple name/value pairs.
 * COMMENT: This is a map, not a "pair". The adt really needs to be renamed some day.
 */
typedef struct pair_tag* pair;
pair pair_new(size_t nelem)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

void pair_free(pair p);

status_t pair_add(pair p, const char *name, const char *value)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

status_t pair_set(pair p, const char *name, const char *value)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

const char *pair_get_name(pair p, size_t i)
    __attribute__((nonnull(1)));

const char *pair_get(pair p, const char *name)
    __attribute__((nonnull(1, 2)));

/* Sometimes we iterate on index to get names. Then there is no
 * need to get the value by name as we have the index anyway.
 * We avoid a lot of strcmps() this way.
 */
const char *pair_get_value_by_index(pair p, size_t i)
    __attribute__((nonnull(1)));

size_t pair_size(pair p)
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
