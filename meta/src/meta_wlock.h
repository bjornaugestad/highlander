/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_WLOCK_H
#define META_WLOCK_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wlock_tag* wlock;

wlock wlock_new(void)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

void wlock_free(wlock p);

status_t wlock_lock(wlock p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t wlock_unlock(wlock p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t wlock_signal(wlock p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t wlock_wait(wlock p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t wlock_broadcast(wlock p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
