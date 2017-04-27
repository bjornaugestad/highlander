/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H


#include <stdbool.h>
#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct threadpool_tag* threadpool;

threadpool threadpool_new(
    size_t num_worker_threads,
    size_t max_queue_size,
    bool block_when_full)
    __attribute__((malloc));

status_t threadpool_add_work(
    threadpool tp,
    void (*initfn)(void*, void*),
    void *initialize_arg,

    void *(*workfn)(void*),
    void *workarg,

    void (*cleanupfn)(void*, void*),
    void *cleanup_arg)
    __attribute__((nonnull(1, 4, 5)));


status_t threadpool_destroy(threadpool tp, bool finish)
    __attribute__((warn_unused_result));

unsigned long threadpool_sum_blocked(threadpool p)
    __attribute__((nonnull(1)));

unsigned long threadpool_sum_discarded(threadpool p)
    __attribute__((nonnull(1)));

unsigned long threadpool_sum_added(threadpool p)
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
