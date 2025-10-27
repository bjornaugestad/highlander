/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
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
#include <stdatomic.h>

#include <meta_common.h>
#include <threadpool.h>

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
    _Atomic unsigned long sum_work_added; // Successfully added to queue
    _Atomic unsigned long sum_blocked;    // How many times we blocked
    _Atomic unsigned long sum_discarded;  // discarded due to queue full and no-block
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
//
// NOTE 20251022: Looks wrong as the work is discarded.
// The work is still in the queue, so ideally threadpool_destroy()
// should let the work be done if finish is true, but since
// the thread just exits, no work will be performed.
// BUG/FIXME/TODO
// OTOH, threadpool_destroy() does not set the shutdown flag until
// the queue is empty, so perhaps we're all good after all? We
// still have that one memleak which proves we're not all good.
//
// I think there are too many states here and a shared condvar.
// We have queue_empty, queue_full, queue_closed, and shutting_down.
// queue_not_full as well. Also, we only broadcast/signal
// queue_not_full and queue_empty even if state is different. Hmm.
// OTOH, we can only cond_wait for one condvar at a time. eventfd FTW?
static void *threadpool_exec_thread(void *arg)
{
    threadpool pool = arg;

    for (;;) {
        // Get the lock so that we can do a cond_wait later
        pthread_mutex_lock(&pool->queue_lock);

        // Wait for entries in the queue
        while (queue_empty(pool) && !pool->shutting_down)
            pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);

        if (pool->shutting_down && queue_empty(pool)) {
            // We're shutting down, just unlock and exit the loop
            pthread_mutex_unlock(&pool->queue_lock);
            return NULL;
        }

        if (queue_empty(pool)) {
            pthread_mutex_unlock(&pool->queue_lock);
            return NULL;
        }

        // Remove one item from the queue
        struct work* wp = pool->queue_head;
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
    assert(nthreads > 0);
    assert(max_queue_size > 0);

    threadpool p = malloc(sizeof *p);
    if (p == NULL)
        return NULL;

    p->threads = calloc(nthreads, sizeof *p->threads);
    if (p->threads == NULL) {
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

    int err;
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
    for (size_t i = 0; i < nthreads; i++) {
        err = pthread_create(&p->threads[i], NULL, threadpool_exec_thread, p);
        if (err) {
            if (!threadpool_destroy(p, 0))
                warning("Unable to destroy thread pool\n");

            return NULL;
        }
    }

    /* Initialize stats counters */
    atomic_init(&p->sum_work_added, 0);
    atomic_init(&p->sum_blocked, 0);
    atomic_init(&p->sum_discarded, 0);
    return p;
}

status_t threadpool_add_work(threadpool pool,
    status_t (*initfn)(void *initarg, void *workarg), void *initarg,
    void *(*workfn)(void*), void *workarg,
    void (*cleanupfn)(void *cleanuparg, void *workarg), void *cleanup_arg)
{
    assert(pool != NULL);
    assert(workfn != NULL);

    int err = pthread_mutex_lock(&pool->queue_lock);
    if (err)
        return fail(err);

    /* Check for available space and what to do if full */
    if (queue_full(pool) ) {
        if (pool->block_when_full) {
            atomic_fetch_add(&pool->sum_blocked, 1);
        }
        else {
            // Can't continue as the queue is full and we cannot block
            pthread_mutex_unlock(&pool->queue_lock);
            atomic_fetch_add(&pool->sum_discarded, 1);
            return fail(ENOSPC);
        }
    }

    // Wait for available space in the queue. This is the 'block when full' part
    while (queue_full(pool) && !pool->shutting_down && !pool->queue_closed) {
        err = pthread_cond_wait(&pool->queue_not_full, &pool->queue_lock);
        if (err)
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
    struct work* wp = malloc(sizeof *wp);
    if (wp == NULL) {
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
        // This _looks_ wrong, but's been running for decades.
        // See also comment above about 'less generic'.
        pool->queue_tail->next = wp;
        pool->queue_tail = wp;
    }

    pool->cur_queue_size++;
    pthread_mutex_unlock(&pool->queue_lock);
    atomic_fetch_add(&pool->sum_work_added, 1);
    return success;
}

status_t threadpool_destroy(threadpool pool, bool finish)
{
    // mimic free(NULL) semantics
    if (pool == NULL)
        return success;

    int err = pthread_mutex_lock(&pool->queue_lock);
    if (err)
        return fail(err);

    pool->queue_closed = true; // stop accepting more work
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

#if 0
    // This is wrong. Do it later, with the lock.
    pool->shutting_down = true;
#endif

    // Wake up any sleeping worker threads so that they
    // check the shutting_down flag
    if ((err = pthread_cond_broadcast(&pool->queue_not_empty)))
        return fail(err);

    // This is done to wake up any producers waiting for queue space. Note that
    // threadpool_add_work() will fail as it tests for the shutting_down flag.
    if ((err = pthread_cond_broadcast(&pool->queue_not_full)))
        return fail(err);

    // Release the lock to allow worker threads to work.
    if ((err = pthread_mutex_unlock(&pool->queue_lock)))
        return fail(err);

#if 1
    // Now get the lock back so we can shut down properly. But will
    // it work at all? I doubt it, we can always wait for queue_empty
    // Get the lock so that we can do a cond_wait later
    pthread_mutex_lock(&pool->queue_lock);

    // Wait for entries in the queue
    while (!queue_empty(pool)) {
        pthread_cond_wait(&pool->queue_empty, &pool->queue_lock);
    }

    pool->shutting_down = true;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);
#endif

    // Now wait for each thread to finish
    // NOTE boa@20251022: We need the lock back, right? Or do we since we
    // block on pthread_join()?
    for (size_t i = 0; i < pool->nthreads; i++) {
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
    free(pool);
    return success;
}

unsigned long threadpool_sum_blocked(threadpool p)
{
    assert(p != NULL);
    return atomic_load(&p->sum_blocked);
}

unsigned long threadpool_sum_discarded(threadpool p)
{
    assert(p != NULL);
    return atomic_load(&p->sum_discarded);
}

unsigned long threadpool_sum_added(threadpool p)
{
    assert(p != NULL);
    return atomic_load(&p->sum_work_added);
}
