/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_POOL_H
#define META_POOL_H

#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pool_tag* pool;

pool pool_new(size_t nelem)
    __attribute__((malloc));

void pool_free(pool p, dtor cleanup);

void  pool_add(pool p, void *resource)
    __attribute__((nonnull(1, 2)));

void *pool_get(pool p)
    __attribute__((nonnull(1)));

void  pool_recycle(pool p, void *resource)
    __attribute__((nonnull(1, 2)));

#ifdef __cplusplus
}
#endif

#endif
