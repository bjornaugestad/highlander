/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H


#include <stdbool.h>
#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct threadpool_tag* threadpool;

threadpool threadpool_new(
    size_t num_worker_threads,
    size_t max_queue_size,
    bool block_when_full)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

status_t threadpool_add_work(
    threadpool tp,
    status_t (*initfn)(void*, void*),
    void *initialize_arg,

    void *(*workfn)(void*),
    void *workarg,

    void (*cleanupfn)(void*, void*),
    void *cleanup_arg)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 4, 5)));


status_t threadpool_destroy(threadpool tp, bool finish)
    __attribute__((warn_unused_result));

unsigned long threadpool_sum_blocked(threadpool p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

unsigned long threadpool_sum_discarded(threadpool p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

unsigned long threadpool_sum_added(threadpool p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
