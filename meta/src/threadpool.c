/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

/*
 * This code is based on the code found in Pthreads Programming,
 * page 99+, with a few bugs removed :-)
 */

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <meta_common.h>
#include <threadpool.h>
#include <meta_atomic.h>

/*
 * The work queue contains 0..max_queue_size instances of this struct.
 * It is basically a single-linked list, where each node contains pointers
 * to functions plus arguments to the functions.
 *
 * initfn and cleanupfn accept two args, their own arg plus
 * the workarg.
 */
struct work {
    void *(*workfn)(void*);
    void *workarg;

    status_t (*initfn)(void*, void*);
    void *initarg;

    void (*cleanupfn)(void*, void*);
    void *cleanup_arg;

    struct work* next;
};

struct threadpool_tag {
    size_t nthreads;
    pthread_t *threads;

    size_t max_queue_size;
    size_t cur_queue_size;
    struct work *queue_head;
    struct work *queue_tail;

    pthread_mutex_t queue_lock;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;
    pthread_cond_t queue_empty;

    bool queue_closed;
    bool shutting_down;
    bool block_when_full;

    // counters used to track and analyze the behaviour of the threadpool.
    // We count how many times we blocked due to full queue, how many work
    // request that's been added to the queue, how many that's in the queue
    // right now, and more...
    atomic_ulong sum_work_added; // Successfully added to queue
    atomic_ulong sum_blocked;    // How many times we blocked
    atomic_ulong sum_discarded;  // discarded due to queue full and no-block
};

static inline bool queue_empty(threadpool tp)
{
    return tp->cur_queue_size == 0;
}

static inline bool queue_full(threadpool tp)
{
    return tp->cur_queue_size == tp->max_queue_size;
}

// We have multiple instances of this function, running as POSIX threads.
// They all wait for work to arrive in the work queue, or for the
// shutting_down flag to be true.
static void *threadpool_exec_thread(void *arg)
{
    struct work* wp;
    threadpool pool = arg;

    for (;;) {
        // Get the lock so that we can do a cond_wait later
        pthread_mutex_lock(&pool->queue_lock);

        // Wait for entries in the queue
        while (queue_empty(pool) && !pool->shutting_down)
            pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);

        if (pool->shutting_down) {
            // We're shutting down, just unlock and exit the loop
            pthread_mutex_unlock(&pool->queue_lock);
            return NULL;
        }

        // Remove one item from the queue
        wp = pool->queue_head;
        pool->cur_queue_size--;

        // Link or clear the queue node pointers, as needed
        if (queue_empty(pool))
            pool->queue_head = pool->queue_tail = NULL;
        else
            pool->queue_head = wp->next;

        if (pool->block_when_full
        && pool->cur_queue_size == pool->max_queue_size - 1) {
            // Tell producer(s) that there now is room in the queue
            pthread_cond_signal(&pool->queue_not_full);
        }

        if (queue_empty(pool))
            pthread_cond_signal(&pool->queue_empty);

        // Unlock the queue so that new entries can be added
        pthread_mutex_unlock(&pool->queue_lock);

        // Call initfn, if present
        if (wp->initfn != 0)
            wp->initfn(wp->initarg, wp->workarg);

        // Do the actual work. We cannot handle or use the return value, as we
        // are a thread _pool_ and each thread will (over time) serve many
        // different requests.
        wp->workfn(wp->workarg);

        // Call cleanup function, if present
        if (wp->cleanupfn != 0)
            wp->cleanupfn(wp->cleanup_arg, wp->workarg);

        free(wp);
    }

    return NULL;
}

threadpool threadpool_new(size_t nthreads, size_t max_queue_size,
    bool block_when_full)
{
    int err;
    size_t i;
    threadpool p;

    assert(nthreads > 0);
    assert(max_queue_size > 0);

    /* Allocate space for the pool */
    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    if ((p->threads = calloc(nthreads, sizeof *p->threads)) == NULL) {
        free(p);
        return NULL;
    }

    p->nthreads = nthreads;
    p->max_queue_size = max_queue_size;
    p->block_when_full = block_when_full;

    p->cur_queue_size = 0;
    p->queue_head = NULL;
    p->queue_tail = NULL;
    p->queue_closed = false;
    p->shutting_down = false;

    if ((err = pthread_mutex_init(&p->queue_lock, NULL))
    ||	(err = pthread_cond_init(&p->queue_not_empty, NULL))
    ||	(err = pthread_cond_init(&p->queue_not_full, NULL))
    ||	(err = pthread_cond_init(&p->queue_empty, NULL))) {
        free(p->threads);
        free(p);
        errno = err;
        return NULL;
    }

    /* Create the worker threads */
    for (i = 0; i < nthreads; i++) {
        err = pthread_create(&p->threads[i], NULL, threadpool_exec_thread, p);
        if (err) {
            if (!threadpool_destroy(p, 0))
                warning("Unable to destroy thread pool\n");

            return NULL;
        }
    }

    /* Initialize stats counters */
    atomic_ulong_init(&p->sum_work_added);
    atomic_ulong_init(&p->sum_blocked);
    atomic_ulong_init(&p->sum_discarded);
    return p;
}

status_t threadpool_add_work(threadpool pool,
    status_t (*initfn)(void *initarg, void *workarg), void *initarg,
    void *(*workfn)(void*), void *workarg,
    void (*cleanupfn)(void *cleanuparg, void *workarg), void *cleanup_arg)
{
    struct work* wp;
    int err;

    assert(pool != NULL);
    assert(workfn != NULL);

    if ((err = pthread_mutex_lock(&pool->queue_lock)))
        return fail(err);

    /* Check for available space and what to do if full */
    if (queue_full(pool) ) {
        if (pool->block_when_full) {
            atomic_ulong_inc(&pool->sum_blocked);
        }
        else {
            // Can't continue as the queue is full and we cannot block
            pthread_mutex_unlock(&pool->queue_lock);
            atomic_ulong_inc(&pool->sum_discarded);
            return fail(ENOSPC);
        }
    }

    // Wait for available space in the queue. This is the 'block when full' part
    while (queue_full(pool) && !pool->shutting_down && !pool->queue_closed) {
        if ((err = pthread_cond_wait(&pool->queue_not_full, &pool->queue_lock)))
            return fail(err);
    }

    // We cannot add more work to a closed queue. The same goes for an
    // application which is shutting down. This isn't really an error, as it
    // will happen in a threaded app.
    if (pool->shutting_down || pool->queue_closed) {
        pthread_mutex_unlock(&pool->queue_lock);
        return fail(EINVAL);
    }

    // Now add a new entry to the queue. Note that this malloc() can be 
    // avoided without performance loss if we make the threadpool less
    // generic. For example, we could add the values to the connection
    // object and totally avoid the malloc/free cycling of objects. The 
    // downside is that then the threadpool would only work with connection
    // objects.
    if ((wp = malloc(sizeof *wp)) == NULL) {
        pthread_mutex_unlock(&pool->queue_lock);
        return failure;
    }

    wp->initfn = initfn;
    wp->initarg = initarg;
    wp->workfn = workfn;
    wp->workarg = workarg;
    wp->cleanupfn = cleanupfn;
    wp->cleanup_arg = cleanup_arg;
    wp->next = NULL;

    if (queue_empty(pool)) {
        pool->queue_tail = pool->queue_head = wp;
        pthread_cond_broadcast(&pool->queue_not_empty);
    }
    else {
        pool->queue_tail->next = wp;
        pool->queue_tail = wp;
    }

    pool->cur_queue_size++;
    pthread_mutex_unlock(&pool->queue_lock);
    atomic_ulong_inc(&pool->sum_work_added);
    return success;
}

status_t threadpool_destroy(threadpool pool, bool finish)
{
    size_t i;
    int err;

    // mimic free(NULL) semantics
    if (pool == NULL)
        return success;

    if ((err = pthread_mutex_lock(&pool->queue_lock)))
        return fail(err);

    if (pool->queue_closed || pool->shutting_down) {
        pthread_mutex_unlock(&pool->queue_lock);
        return fail(EINVAL); // The queue is already closed
    }

    pool->queue_closed = true;
    if (finish) {
        // Wait for the worker threads to perform all work in the queue.
        while (!queue_empty(pool)) {
            err = pthread_cond_wait(&pool->queue_empty, &pool->queue_lock);
            if (err) {
                // Unable to condwait for the other threads to finish
                pthread_mutex_unlock(&pool->queue_lock);
                return fail(err);
            }
        }
    }

    pool->shutting_down = true;
    if ((err = pthread_mutex_unlock(&pool->queue_lock)))
        return fail(err);

    // Wake up any sleeping worker threads so that they
    // check the shutting_down flag
    if ((err = pthread_cond_broadcast(&pool->queue_not_empty)))
        return fail(err);

    // This is done to wake up any producers waiting for queue space. Note that
    // threadpool_add_work() will fail as it tests for the shutting_down flag.
    if ((err = pthread_cond_broadcast(&pool->queue_not_full)))
        return fail(err);

    // Now wait for each thread to finish
    for (i = 0; i < pool->nthreads; i++) {
        // Don't join NULL threads.
        if (pool->threads[i] == 0)
            continue;

        if ((err = pthread_join(pool->threads[i], NULL)))
            return fail(err);
    }

    free(pool->threads);

    // Free the queue. The queue should be empty since all the worker
    // threads remove entries from here, still we free it. The queue
    // may have been closed just after a new entry was added, but before
    // a worker thread could process the work.
    while (pool->queue_head != NULL) {
        struct work* wp = pool->queue_head;
        pool->queue_head = pool->queue_head->next;
        free(wp);
    }

    pthread_mutex_destroy(&pool->queue_lock);
    atomic_ulong_destroy(&pool->sum_work_added);
    atomic_ulong_destroy(&pool->sum_blocked);
    atomic_ulong_destroy(&pool->sum_discarded);
    free(pool);
    return success;
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
