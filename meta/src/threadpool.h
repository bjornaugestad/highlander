/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
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

#ifdef __cplusplus
extern "C" {
#endif

/* Declaration of our threadpool ADT */
typedef struct threadpool_tag* threadpool;

/*
 * @file
 * Creates a new thread pool and returns zero if successful.
 * Returns an error code if not.
 */

threadpool threadpool_new(
	size_t num_worker_threads,
	size_t max_queue_size,
	int block_when_full);

/*
 * Adds work to the queue. The work will be performed by the work_func,
 * After the work_func has been executed, the cleanup function will
 * be executed. This way you can pass e.g. allocated memory to work_func
 * and cleanup_func can free it.
 * Note that initialize function and the cleanup function takes 2 parameters,
 * cleanup_arg and work_arg, in that order.
 *
 * This function sets errno to ENOSPC if the work queue is full and
 * the pool is set to not block.
 */
int threadpool_add_work(
	threadpool tp,
	void (*initialize)(void*, void*),
	void *initialize_arg,

	void *(*work_func)(void*),
	void *work_arg,
	void (*cleanup_func)(void*, void*),
	void *cleanup_arg);


int threadpool_destroy(threadpool tp, unsigned int finish);

/* Performance counters: Returns the number of work requests
 * successfully added, discarded or blocked since startup.
 */
unsigned long threadpool_sum_blocked(threadpool p);
unsigned long threadpool_sum_discarded(threadpool p);
unsigned long threadpool_sum_added(threadpool p);

#ifdef __cplusplus
}
#endif

#endif
