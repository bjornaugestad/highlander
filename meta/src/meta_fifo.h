/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_FIFO_H
#define META_FIFO_H

#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fifo_tag* fifo;

fifo fifo_new(size_t size)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void fifo_free(fifo p, dtor dtor_fn);

status_t fifo_lock(fifo p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t fifo_unlock(fifo p)
    __attribute__((nonnull));
    // Too many corner cases. __attribute__((warn_unused_result));

status_t fifo_add(fifo p, void *data)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

size_t fifo_nelem(fifo p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

size_t fifo_free_slot_count(fifo p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

void *fifo_get(fifo p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

void *fifo_peek(fifo p, size_t i)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t fifo_write_signal(fifo p, void *data)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t fifo_wait_cond(fifo p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t fifo_wake(fifo p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t fifo_signal(fifo p)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
