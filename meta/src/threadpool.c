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

// 20251119: Work queue's been replaced with an array of pointers to
// struct work objects, as well as a state array(in_use). No more malloc
// and no more queue. Things look solid even if we hold the lock for
// shorter periods. 
//
// Naming is still off and will be fixed in a separate commit.
struct work {
    void *(*workfn)(void*);
    void *workarg;

    status_t (*initfn)(void*, void*);
    void *initarg;

    void (*cleanupfn)(void*, void*);
    void *cleanup_arg;
};

struct threadpool_tag {
    size_t nthreads;
    pthread_t *threads;

    size_t max_queue_size;
    size_t cur_queue_size;

    // Allocate this once in _new() and free it in _destroy().
    // Same goes for the in_use array.
    struct work **queue;

    // For performance reasons, we need three states. A slot entry may be
    // free to use by _add_work(), running as in used by work_thread(), or
    // waiting to be executed by work_thread().
    unsigned *in_use;
    #define QUEUE_UNUSED 0x01
    #define QUEUE_RUNNING 0x02
    #define QUEUE_WAITING 0x04


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
// seccomp notes 20251028: We can't really drop permissions in
// here as that affects the callback functions and we don't know
// what they need. They can basically do anything.
//
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

        // Find pending work
        struct work* wp = NULL;
        size_t idx;
        for (idx = 0; idx < pool->max_queue_size; idx++) {
            if (pool->in_use[idx] == QUEUE_WAITING) {
                wp = pool->queue[idx];
                break;
            }
        }

        // Did we find work? If not, just continue. 
        if (wp == NULL) {
            pthread_mutex_unlock(&pool->queue_lock);
            continue;
        }

        // Unlock the queue so that new entries can be added
        pool->in_use[idx] = QUEUE_RUNNING;
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

        // Reaquire the lock so we can toggle the flag. If queue is full, we must
        // also broadcast that there's room.
        pthread_mutex_lock(&pool->queue_lock);
        pool->in_use[idx] = QUEUE_UNUSED;
        pool->cur_queue_size--;

        if (pool->block_when_full
        && pool->cur_queue_size == pool->max_queue_size - 1) {
            // Tell producer(s) that there now is room in the queue
            pthread_cond_signal(&pool->queue_not_full);
        }

        if (queue_empty(pool))
            pthread_cond_signal(&pool->queue_empty);

        if (queue_full(pool))
            pthread_cond_broadcast(&pool->queue_not_full);

        pthread_mutex_unlock(&pool->queue_lock);
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
        goto memerr;

    p->threads = calloc(nthreads, sizeof *p->threads);
    if (p->threads == NULL)
        goto memerr;

    // Allocate room for pointers
    p->queue = calloc(max_queue_size, sizeof *p->queue);
    if (p->queue == NULL)
        goto memerr;

    // allocate room for work structs
    for (size_t i = 0; i < max_queue_size; i++) {
        p->queue[i] = malloc(sizeof *p->queue[i]);
        if (p->queue[i] == NULL)
            goto memerr;
    }

    p->in_use = calloc(max_queue_size, sizeof *p->in_use);
    if (p->in_use == NULL)
        goto memerr;

    for (size_t i = 0; i < max_queue_size; i++)
        p->in_use[i] = QUEUE_UNUSED;

    p->nthreads = nthreads;
    p->max_queue_size = max_queue_size;
    p->block_when_full = block_when_full;

    p->cur_queue_size = 0;
    p->queue_closed = false;
    p->shutting_down = false;

    int err;
    if ((err = pthread_mutex_init(&p->queue_lock, NULL))
    ||	(err = pthread_cond_init(&p->queue_not_empty, NULL))
    ||	(err = pthread_cond_init(&p->queue_not_full, NULL))
    ||	(err = pthread_cond_init(&p->queue_empty, NULL))) {
        free(p->in_use);
        if (p->queue) {
            for (size_t i = 0; i < max_queue_size; i++)
                free(p->queue[i]);
            free(p->queue);
        }
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

memerr:
    if (p != NULL) {
        free(p->threads);

        if (p->queue != NULL) {
            for (size_t i = 0; i < max_queue_size; i++) {
                free(p->queue[i]);
            }

            free(p->queue);
        }

        free(p->in_use);
        free(p);
    }

    return NULL;
}

status_t threadpool_add_work(threadpool pool,
    status_t (*initfn)   (void *initarg, void *workarg), void *initarg,
    void *   (*workfn)   (void*), void *workarg,
    void     (*cleanupfn)(void *cleanuparg, void *workarg), void *cleanup_arg)
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

    // Find an unused slot or die trying
    struct work* wp = NULL;
    for (size_t i = 0; i < pool->max_queue_size; i++) {
        if (pool->in_use[i] == QUEUE_UNUSED) {
            wp = pool->queue[i];
            pool->in_use[i] = QUEUE_WAITING;
            break;
        }
    }

    if (wp == NULL) {
        // We didn't find a free slot. That's odd since the queue is not full and we hold
        // the lock. What's going on?.
        // Return gracefully, but WTF?
        die("No free slots!\n");
        pthread_mutex_unlock(&pool->queue_lock);
        return failure;
    }

    wp->initfn = initfn;
    wp->initarg = initarg;
    wp->workfn = workfn;
    wp->workarg = workarg;
    wp->cleanupfn = cleanupfn;
    wp->cleanup_arg = cleanup_arg;

    if (queue_empty(pool))
        pthread_cond_broadcast(&pool->queue_not_empty);

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

    // Now get the lock back so we can shut down properly.
    pthread_mutex_lock(&pool->queue_lock);

    // Wait for entries in the queue
    while (!queue_empty(pool)) {
        pthread_cond_wait(&pool->queue_empty, &pool->queue_lock);
    }

    pool->shutting_down = true;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);

    // Now wait for each thread to finish
    for (size_t i = 0; i < pool->nthreads; i++) {
        // Don't join NULL threads.
        if (pool->threads[i] == 0)
            continue;

        if ((err = pthread_join(pool->threads[i], NULL)))
            return fail(err);
    }

    free(pool->threads);
    if (pool->queue != NULL) {
        for (size_t i = 0; i < pool->max_queue_size; i++)
            free(pool->queue[i]);

        free(pool->queue);
    }
    free(pool->in_use);

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

#ifdef CHECK_THREADPOOL
#include <string.h>
#include <meta_pool.h>

/*
 * Food for thought. 
 * - How do we "get" and "return" work-instances from the array? We need to know
 *   if the object is in use or not. Obvious solution? array of bool flags. in_use(true/false)
 *
 * - What about sequencing of work? The queue was a FIFO queue. The array will be 
 *   an array. We can always treat it as a ring buffer, but that adds complexity and
 *   doesn't give us much functionality.
 *
 * - How to test? We really should add a test program in this file. 
 *   That should be step one, before any changes!
 *
 * - What to test? 
 *   a) adding work.
 *   b) Fill array to max_queue_size
 *   c) Do work. 
 *   d) Proper shutdown
 *   e) block when full (yes/no)
 *   f) Having multiple threads trying to add work to a full queue.
 */

/*
 * We start with the basics: create and destroy
 */
static void create_and_destroy(void)
{
    size_t nworkers = 10, max_queue_size = 20;
    bool block_when_full = false;

    // test 1: nothing special
    threadpool tp = threadpool_new(nworkers, max_queue_size, block_when_full);
    if (tp == NULL)
        die("threadpool_new\n");

    bool finish_all_work = false;
    status_t ok = threadpool_destroy(tp, finish_all_work);
    if (!ok)
        die("threadpool_destroy\n");

    // test 2: let's block when full
    block_when_full = true;
    tp = threadpool_new(nworkers, max_queue_size, block_when_full);
    if (tp == NULL)
        die("threadpool_new\n");

    ok = threadpool_destroy(tp, finish_all_work);
    if (!ok)
        die("threadpool_destroy\n");

    // test 3: let's finish all work
    tp = threadpool_new(nworkers, max_queue_size, block_when_full);
    if (tp == NULL)
        die("threadpool_new\n");

    finish_all_work = true;
    ok = threadpool_destroy(tp, finish_all_work);
    if (!ok)
        die("threadpool_destroy\n");
}

// worker functions so we can add some work and execute too
// The init function is called once, just before we call the workfn().
// Why? I don't remember. It may be because we want to offload init/exit code
// from the workfn itself.
// We add a silly struct just to have some data to play with and so
// valgrind and sanitizers can detect issues.
struct dummy {
    char *str;
};

static status_t initfn(void *initarg, void *workarg)
{
    assert(initarg != NULL);
    assert(workarg != NULL);

    const char *initstr = initarg;
    struct dummy *p = workarg;

    size_t nbytes = strlen(initstr) + 1;
    p->str = malloc(nbytes);
    if (p->str == NULL)
        return false;

    memcpy(p->str, initstr, nbytes);
    free(initarg);
    return success;
}

// This is the function performing the actual work, like serving a socket.
// It can do anything (or nothing, like here)
static void *workfn(void *workarg)
{
    assert(workarg != NULL);

    struct dummy *p = workarg;
    assert(p->str != NULL);

    // It should be possible to read the integer value in the str member.
    // if not, we die.
    int i;
    if (sscanf(p->str, "%d", &i) != 1)
        die("scanf: %s\n", p->str);
    (void)i;

    return NULL; // Which is fine.
}

// cleanuparg is the metapool, workarg is the struct dummy object.
static void cleanupfn(void *cleanuparg, void *workarg)
{
    assert(cleanuparg != NULL);
    assert(workarg != NULL);

    // Free the memory allocated by initfn()
    struct dummy *p = workarg;
    assert(p->str != NULL);
    free(p->str);
    p->str = NULL;

    pool dpool = cleanuparg;
    if (!pool_recycle(dpool, p))
        die("pool_recycle");
}

static void add_work(void)
{
    size_t nworkers = 10, max_queue_size = 20;
    bool block_when_full = true;
    size_t dummypool_size = 200;

    threadpool tp = threadpool_new(nworkers, max_queue_size, block_when_full);
    if (tp == NULL)
        die("threadpool_new\n");

    // We need buffers to work on which can be freed later.
    pool dummypool = pool_new(dummypool_size);
    if (dummypool == NULL)
        die("pool_new");

    for (size_t i = 0; i < dummypool_size; i++) {
        struct dummy *p = malloc(sizeof *p);
        if (!pool_add(dummypool, p))
            die("pool_add");
    }

    // Now add a bunch of work to the thread. It should block, so no need to test for errors
    for (int i = 0; i < 100; i++) {
        struct dummy *pdummy;

        if (!pool_get(dummypool, (void**)&pdummy))
            die("pool_get");

        // freed by initfn
        char *initarg = malloc(16);
        sprintf(initarg, "%d", i);

        status_t ok = threadpool_add_work(tp, initfn, initarg, workfn, pdummy, cleanupfn, dummypool);
        if (!ok)
            die("Could not add work to blocking queue");
    }


    // Now destroy the pool, but finish all work.
    bool finish_all_work = true;
    status_t ok = threadpool_destroy(tp, finish_all_work);
    if (!ok)
        die("threadpool_destroy\n");

    // Free the dummypool too
    pool_free(dummypool, free);
}


int main(void)
{
    create_and_destroy();
    add_work();
    return 0;
}

#endif
