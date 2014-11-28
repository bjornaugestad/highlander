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

/*
 * This code is based on the code found in Pthreads Programming,
 * page 99+, with a few bugs removed :-)
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#ifdef __sun
#	define _POSIX_PTHREAD_SEMANTICS
#endif
#include <pthread.h>

#include <meta_common.h>
#include <threadpool.h>
#include <meta_atomic.h>

/*
 * The queue contains this struct. It is basically a linked list, where
 * each node contains pointers to functions plus arguments to the
 * functions.
 */
struct threadpool_work {
	void* (*work_func)(void*);
	void* work_arg;

	void (*initialize)(void*, void*);
	void* initialize_arg;

	void (*cleanup)(void*, void*);
	void* cleanup_arg;
	struct threadpool_work* next;
};

/*
 * Implementation of the threadpool ADT.
 */
struct threadpool_tag {
	size_t num_threads;
	size_t max_queue_size;
	int block_when_full;

	size_t cur_queue_size;
	struct threadpool_work* queue_head;
	struct threadpool_work* queue_tail;
	pthread_t* threads;
	pthread_mutex_t queue_lock;
	pthread_cond_t queue_not_empty;
	pthread_cond_t queue_not_full;
	pthread_cond_t queue_empty;
	int queue_closed;
	int shutdown;

	/* counters used to track and analyze the behaviour of the threadpool.
	 * We count how many times we blocked due to full queue,
	 * how many work request that's been added to the queue, how many
	 * that's in the queue right now, and more...
	 */
	atomic_ulong sum_work_added; /* Successfully added to queue */
	atomic_ulong sum_blocked; /* How many times we blocked */
	atomic_ulong sum_discarded; /* discarded due to queue full and no-block */
};


static int queue_empty(threadpool tp)
{
	return tp->cur_queue_size == 0;
}

static int queue_full(threadpool tp)
{
	return tp->cur_queue_size == tp->max_queue_size;
}

static void* threadpool_exec_thread(void* arg)
{
	struct threadpool_work* wp;
	threadpool pool = (threadpool)arg;

	pthread_detach(pthread_self());
	for (;;) {
		/* Get the lock so that we can do a cond_wait later */
		pthread_mutex_lock(&pool->queue_lock);

		/* Wait until there is something to do in the queue */
		while (queue_empty(pool) && !pool->shutdown) {
			pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);
		}

		if (pool->shutdown) {
			/* We're shutting down, just unlock and exit the loop */
			pthread_mutex_unlock(&pool->queue_lock);
			return NULL;
		}

		/* Remove one item from the queue */
		wp = pool->queue_head;
		pool->cur_queue_size--;

		if (queue_empty(pool))
			pool->queue_head = pool->queue_tail = NULL;
		else
			pool->queue_head = wp->next;

		if (pool->block_when_full
		&& pool->cur_queue_size == (pool->max_queue_size - 1)) {
			/* Tell producer(s) that there now is room in the queue */
			pthread_cond_signal(&pool->queue_not_full);
		}

		if (queue_empty(pool))
			pthread_cond_signal(&pool->queue_empty);

		/* Unlock the queue so that new entries can be added */
		pthread_mutex_unlock(&pool->queue_lock);

		/* Perform initialization, action and cleanup */
		if (wp->initialize != NULL)
			wp->initialize(wp->initialize_arg, wp->work_arg);


		/* Do the actual work. We cannot handle or use the
		 * return value, as we are a thread _pool_ and each
		 * thread will (over time) serve many different requests.
		 */
		(void)wp->work_func(wp->work_arg);

		if (wp->cleanup != NULL)
			wp->cleanup(wp->cleanup_arg, wp->work_arg);

		free(wp);
	}

	/* This function runs until pool->shutdown is true, but the HP-UX
	 * compiler isn't smart enough to detect this. The line below is
	 * there just to avoid a warning.
	 */
	return NULL;
}

threadpool threadpool_new(
	unsigned int num_worker_threads,
	unsigned int max_queue_size,
	int block_when_full)
{
	int error;
	unsigned int i;
	threadpool tpool;

	assert(num_worker_threads > 0);
	assert(max_queue_size > 0);

	/* Allocate space for the pool */
	if ((tpool = malloc(sizeof(struct threadpool_tag))) == NULL
	|| (tpool->threads = malloc(sizeof(pthread_t) * num_worker_threads)) == NULL) {
		free(tpool);
		return NULL;
	}

	tpool->num_threads = num_worker_threads;
	tpool->max_queue_size = max_queue_size;
	tpool->block_when_full = block_when_full;

	tpool->cur_queue_size = 0;
	tpool->queue_head = NULL;
	tpool->queue_tail = NULL;
	tpool->queue_closed = 0;
	tpool->shutdown = 0;

	if ((error = pthread_mutex_init(&tpool->queue_lock, NULL))
	||	(error = pthread_cond_init(&tpool->queue_not_empty, NULL))
	||	(error = pthread_cond_init(&tpool->queue_not_full, NULL))
	||	(error = pthread_cond_init(&tpool->queue_empty, NULL))) {
		free(tpool->threads);
		free(tpool);
		errno = error;
		return NULL;
	}

	/* Create the threads */
	for (i = 0; i < num_worker_threads; i++) {
		error = pthread_create(
			&tpool->threads[i],
			NULL,
			threadpool_exec_thread,
			tpool);

		if (error) {
			/* NOTE: Should we destroy the threads as well? */
			free(tpool->threads);
			free(tpool);
			errno = error;
			return NULL;
		}
	}

	/* Initialize stats counters */
	atomic_ulong_init(&tpool->sum_work_added);
	atomic_ulong_init(&tpool->sum_blocked);
	atomic_ulong_init(&tpool->sum_discarded);
	return tpool;
}

int threadpool_add_work(
	threadpool pool,
	void (*initialize)(void*, void*),
	void *initialize_arg,

	void* (*work_func)(void*),
	void* work_arg,
	void (*cleanup)(void*, void*),
	void* cleanup_arg)
{
	struct threadpool_work* wp;
	int error;

	assert(NULL != pool);
	assert(NULL != work_func);

	if ((error = pthread_mutex_lock(&pool->queue_lock))) {
		errno = error;
		return 0;
	}

	/* Check for available space and what to do if full */
	if (queue_full(pool) ) {
		if (!pool->block_when_full) {
			/* Cannot continue as the queue is full and we cannot block */
			pthread_mutex_unlock(&pool->queue_lock);
			atomic_ulong_inc(&pool->sum_discarded);
			errno = ENOSPC;
			return 0;
		}
		else {
			atomic_ulong_inc(&pool->sum_blocked);
		}
	}

	/* Wait until there is available space in the queue */
	while (queue_full(pool) && !pool->shutdown && !pool->queue_closed) {
		if ((error = pthread_cond_wait(&pool->queue_not_full, &pool->queue_lock))) {
			errno = error;
			return 0;
		}
	}

	/*
	 * We cannot add more work to a closed queue. The same
	 * goes for an application which is shutting down. This isn't
	 * really an error, as it will happen in a threaded app.
	 */
	if (pool->shutdown || pool->queue_closed) {
		pthread_mutex_unlock(&pool->queue_lock);
		errno = EINVAL;
		return 0;
	}

	/* Now add a new entry to the queue */
	if ((wp = malloc(sizeof *wp)) == NULL) {
		pthread_mutex_unlock(&pool->queue_lock);
		return 0;
	}

	wp->initialize = initialize;
	wp->initialize_arg = initialize_arg;
	wp->work_func = work_func;
	wp->work_arg = work_arg;
	wp->cleanup = cleanup;
	wp->cleanup_arg = cleanup_arg;
	wp->next = NULL;

	if (queue_empty(pool)) {
		pool->queue_tail = pool->queue_head = wp;
		/* 20031231 - Changed from signal to broadcast */
		pthread_cond_broadcast(&pool->queue_not_empty);
	}
	else {
		pool->queue_tail->next = wp;
		pool->queue_tail = wp;
	}

	pool->cur_queue_size++;
	pthread_mutex_unlock(&pool->queue_lock);
	atomic_ulong_inc(&pool->sum_work_added);
	return 1;
}

int threadpool_destroy(threadpool pool, unsigned int finish)
{
	unsigned int i;
	int error;

	if (pool == NULL)
		return 1;

	if ((error = pthread_mutex_lock(&pool->queue_lock))) {
		errno = error;
		return 0;
	}

	if (pool->queue_closed || pool->shutdown) {
		/* The queue is already closed */
		pthread_mutex_unlock(&pool->queue_lock);
		errno = EINVAL;
		return 0;
	}

	pool->queue_closed = 1;
	if (finish) {
		while (!queue_empty(pool)) {
			error = pthread_cond_wait(&pool->queue_empty, &pool->queue_lock);
			if (error) {
				/* Unable to condwait for the other threads to finish */
				pthread_mutex_unlock(&pool->queue_lock);
				errno = error;
				return 0;
			}
		}
	}

	pool->shutdown = 1;
	if ((error = pthread_mutex_unlock(&pool->queue_lock))) {
		errno = error;
		return 0;
	}

	/* Wake up any sleeping worker threads so that they
	 * check the shutdown flag */
	if ((error = pthread_cond_broadcast(&pool->queue_not_empty))) {
		errno = error;
		return 0;
	}

	/* This is done to wake up any producers waiting for queue space.
	 * Note that threadpool_add_work will fail as it tests for
	 * the shutdown flag
	 */
	if ((error = pthread_cond_broadcast(&pool->queue_not_full))) {
		errno = error;
		return 0;
	}

	/* Now wait for each thread to finish */
	for (i = 0; i < pool->num_threads; i++) {
		if ((error = pthread_join(pool->threads[i], NULL))) {
			errno = error;
			return 0;
		}
	}

	free(pool->threads);

	/*
	 * Free the queue. The queue should be empty since all the worker
	 * threads remove entries from here, still we free it. Never know
	 * if a thread crashed.
	 */
	while (pool->queue_head != NULL) {
		struct threadpool_work* wp = pool->queue_head;
		pool->queue_head = pool->queue_head->next;
		free(wp);
	}

	pthread_mutex_destroy(&pool->queue_lock);
	atomic_ulong_destroy(&pool->sum_work_added);
	atomic_ulong_destroy(&pool->sum_blocked);
	atomic_ulong_destroy(&pool->sum_discarded);
	free(pool);
	return 1;
}

unsigned long threadpool_sum_blocked(threadpool p)
{
	assert(p != NULL);
	return atomic_ulong_get(&p->sum_blocked);
}

unsigned long threadpool_sum_discarded(threadpool p)
{
	assert(p != NULL);
	return atomic_ulong_get(&p->sum_discarded);
}

unsigned long threadpool_sum_added(threadpool p)
{
	assert(p != NULL);
	return atomic_ulong_get(&p->sum_work_added);
}
