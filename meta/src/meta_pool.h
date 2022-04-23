/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
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
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void pool_free(pool p, dtor cleanup);

void pool_add(pool p, void *resource)
    __attribute__((nonnull));

status_t pool_get(pool p, void **ppres)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t pool_recycle(pool p, void *resource)
    __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
