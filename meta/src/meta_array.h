/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_ARRAY_H
#define META_ARRAY_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct array_tag* array;

array array_new(size_t nmemb, int can_grow)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void  array_free(array a, dtor cln);

size_t array_nelem(array a)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

void * array_get(array a, size_t ielem)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

status_t array_add(array a, void *elem)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

status_t array_extend(array a, size_t nmemb)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
