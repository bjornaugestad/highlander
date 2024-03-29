/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_CACHE_H
#define META_CACHE_H

#include <stddef.h>
#include <meta_common.h>

typedef struct cache_tag* cache;

cache cache_new(size_t nelem, size_t hotlist_nelem, size_t cb)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

void cache_free(cache c, dtor cleanup);

int cache_exists(cache c, size_t id)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cache_add(cache c, size_t id, void *data, size_t cb, int pin)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cache_get(cache c, size_t id, void** pdata, size_t* pcb)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cache_remove(cache c, size_t id)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

#if 0
void cache_invalidate(cache c);
#endif


#endif
