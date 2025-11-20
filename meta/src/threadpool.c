/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
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
// struct work objects, as well as a state. No more malloc
// and no more queue. Things look solid even if we hold the lock for
// shorter periods. 
//
// Naming is still off and will be fixed in a separate commit.
struct work {
    // For performance reasons, we need three states. A slot entry may be
    // free to use by _add_work(), running as in used by exec_thread(), or
    // waiting to be executed by exec_thread().
    unsigned state;
    #define SLOT_UNUSED 0x01
    #define SLOT_RUNNING 0x02
    #define SLOT_WAITING 0x04

    void *(*workfn)(void*);
    void *workarg;

    status_t (*initfn)(void*, void*);
    void *initarg;

    void (*cleanupfn)(void*, void*);
    void *cleanup_arg;
};

struct threadpool_tag {
    size_t nworkers;
    pthread_t *workers;

    size_t capacity;
    size_t nentries;

    // Allocate this once in _new() and free it in _destroy().
    struct work **queue;

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
    return tp->nentries == 0;
}

static inline bool queue_full(threadpool tp)
{
    return tp->nentries == tp->capacity;
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
        struct work *wp = NULL;
        size_t idx;
        for (idx = 0; idx < pool->capacity; idx++) {
            if (pool->queue[idx]->state == SLOT_WAITING) {
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
        pool->queue[idx]->state = SLOT_RUNNING;
        pthread_mutex_unlock(&pool->queue_lock);

        // Call initfn, if present. 
        // TODO: We should test the return code. Yeah, but if we do and
        // initfn() fails, then what?
        // 1. we cannot exit the thread. That'd be disastrous
        // 2. There's no way to return the status to anyone.
        // So what can we do? We can skip the calls to workfn and cleanupfn
        // Assume that initfn is atomic and cleanup is not needed if initfn
        // fails.
        status_t status = success;
        if (wp->initfn != 0)
            status = wp->initfn(wp->initarg, wp->workarg);

        if (status == success) {
            // Do the actual work. We cannot handle or use the return value, as
            // we are a thread _pool_ and each thread will (over time) serve
            // many different requests.
            wp->workfn(wp->workarg);

            // Call cleanup function, if present
            if (wp->cleanupfn != 0)
                wp->cleanupfn(wp->cleanup_arg, wp->workarg);
        }

        // Reaquire the lock so we can toggle the flag. If queue is full, we
        // must also broadcast that there's room.
        pthread_mutex_lock(&pool->queue_lock);
        pool->queue[idx]->state = SLOT_UNUSED;
        pool->nentries--;

        // Tell producer(s) that there now is room in the queue
        if (pool->block_when_full && !queue_full(pool))
            pthread_cond_signal(&pool->queue_not_full);

        if (queue_empty(pool))
            pthread_cond_signal(&pool->queue_empty);

        pthread_mutex_unlock(&pool->queue_lock);
    }

    return NULL;
}

// Local helper to avoid redundant code
static void free_mem(threadpool p)
{
    if (p != NULL) {
        free(p->workers);

        if (p->queue != NULL) {
            for (size_t i = 0; i < p->capacity; i++)
                free(p->queue[i]);

            free(p->queue);
        }

        free(p);
    }
}

threadpool threadpool_new(size_t nworkers, size_t capacity, bool block_when_full)
{
    assert(nworkers > 0);
    assert(capacity > 0);

    threadpool p = malloc(sizeof *p);
    if (p == NULL)
        goto memerr;

    p->nworkers = nworkers;
    p->capacity = capacity;
    p->block_when_full = block_when_full;

    p->nentries = 0;
    p->queue_closed = false;
    p->shutting_down = false;

    p->workers = calloc(nworkers, sizeof *p->workers);
    if (p->workers == NULL)
        goto memerr;

    // Allocate room for pointers
    p->queue = calloc(capacity, sizeof *p->queue);
    if (p->queue == NULL)
        goto memerr;

    // allocate room for work structs
    for (size_t i = 0; i < capacity; i++) {
        p->queue[i] = calloc(1, sizeof *p->queue[i]);
        if (p->queue[i] == NULL)
            goto memerr;
    }

    for (size_t i = 0; i < capacity; i++)
        p->queue[i]->state = SLOT_UNUSED;

    int err;
    if ((err = pthread_mutex_init(&p->queue_lock, NULL))
    ||	(err = pthread_cond_init(&p->queue_not_empty, NULL))
    ||	(err = pthread_cond_init(&p->queue_not_full, NULL))
    ||	(err = pthread_cond_init(&p->queue_empty, NULL))) {
        free_mem(p);
        errno = err;
        return NULL;
    }

    // Create the worker threads
    for (size_t i = 0; i < nworkers; i++) {
        err = pthread_create(&p->workers[i], NULL, threadpool_exec_thread, p);
        if (err) {
            if (!threadpool_destroy(p, 0))
                warning("Unable to destroy thread pool\n");

            return NULL;
        }
    }

    // Initialize stats counters
    atomic_init(&p->sum_work_added, 0);
    atomic_init(&p->sum_blocked, 0);
    atomic_init(&p->sum_discarded, 0);
    return p;

memerr:
    free_mem(p);
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

    // We cannot add more work to a closed queue. The same goes for a process
    // which is shutting down. This isn't really an error, as it
    // will happen in a threaded process.
    if (pool->shutting_down || pool->queue_closed) {
        pthread_mutex_unlock(&pool->queue_lock);
        return fail(EINVAL);
    }

    // Find an unused slot or die trying
    struct work *wp = NULL;
    for (size_t i = 0; i < pool->capacity; i++) {
        if (pool->queue[i]->state == SLOT_UNUSED) {
            wp = pool->queue[i];
            wp->state = SLOT_WAITING;
            break;
        }
    }

    if (wp == NULL) {
        // We didn't find a free slot. That's odd since the queue is not full
        // and we hold the lock. What's going on? Return gracefully, but WTF?
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

    pool->nentries++;
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

    // stop accepting more work
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
    // Wait for the worker threads to drain the queue
    pthread_mutex_lock(&pool->queue_lock);

    while (!queue_empty(pool))
        pthread_cond_wait(&pool->queue_empty, &pool->queue_lock);

    pool->shutting_down = true;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);

    // Now wait for each thread to finish
    for (size_t i = 0; i < pool->nworkers; i++) {
        // Don't join NULL threads.
        if (pool->workers[i] == 0)
            continue;

        if ((err = pthread_join(pool->workers[i], NULL)))
            return fail(err);
    }

    pthread_mutex_destroy(&pool->queue_lock);
    free_mem(pool);
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
 *
 * - How to test? We really should add a test program in this file. 
 *   That should be step one, before any changes!
 *
 * - What to test? 
 *   a) adding work.
 *   b) Fill array to capacity
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
    size_t nworkers = 10, capacity = 20;
    bool block_when_full = false;

    // test 1: nothing special
    threadpool tp = threadpool_new(nworkers, capacity, block_when_full);
    if (tp == NULL)
        die("threadpool_new\n");

    bool finish_all_work = false;
    status_t ok = threadpool_destroy(tp, finish_all_work);
    if (!ok)
        die("threadpool_destroy\n");

    // test 2: let's block when full
    block_when_full = true;
    tp = threadpool_new(nworkers, capacity, block_when_full);
    if (tp == NULL)
        die("threadpool_new\n");

    ok = threadpool_destroy(tp, finish_all_work);
    if (!ok)
        die("threadpool_destroy\n");

    // test 3: let's finish all work
    tp = threadpool_new(nworkers, capacity, block_when_full);
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
//
// We add a silly struct just to have some data to play with and so
// valgrind and sanitizers can detect issues. The dummy objects are
// kept in a pool, and the functions manipulate the contents, but not
// the dummy pointer itself.
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
    // if not, we die. This is just a simple sanity check.
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
    size_t nworkers = 10, capacity = 20;
    bool block_when_full = true;
    size_t dummypool_size = 20000;
    status_t ok;

    threadpool tp = threadpool_new(nworkers, capacity, block_when_full);
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

    // Now add a bunch of work to the thread. 
    for (int i = 0; i < (int)dummypool_size * 2; i++) {
        struct dummy *pdummy;

        if (!pool_get(dummypool, (void**)&pdummy))
            die("pool_get");

        // freed by initfn
        char *initarg = malloc(16);
        sprintf(initarg, "%d", i);

        ok = threadpool_add_work(tp, initfn, initarg, workfn, pdummy, cleanupfn, dummypool);
        if (!ok)
            die("Could not add work to blocking queue");
    }


    // Now destroy the pool, but finish all work.
    bool finish_all_work = true;
    ok = threadpool_destroy(tp, finish_all_work);
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
