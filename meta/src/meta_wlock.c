/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_wlock.h>

/*
 * Implementation of the meta_wlock ADT.
 */
struct wlock_tag {
    pthread_mutexattr_t ma;
    pthread_mutex_t lock;
    pthread_cond_t condvar;
};

wlock wlock_new(void)
{
    wlock p;

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    pthread_mutexattr_init(&p->ma);
    pthread_mutexattr_settype(&p->ma, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&p->lock, &p->ma);
    pthread_cond_init(&p->condvar, NULL);

    return p;
}

void wlock_free(wlock p)
{
    if (p != NULL) {
        pthread_mutex_destroy(&p->lock);
        pthread_mutexattr_destroy(&p->ma);
        pthread_cond_destroy(&p->condvar);
        free(p);
    }
}

status_t wlock_lock(wlock p)
{
    int err;

    assert(p != NULL);

    if ((err = pthread_mutex_lock(&p->lock)))
        return fail(err);

    return success;
}

status_t wlock_unlock(wlock p)
{
    int err;

    assert(p != NULL);

    if ((err = pthread_mutex_unlock(&p->lock)))
        return fail(err);

    return success;
}

status_t wlock_signal(wlock p)
{
    int err;

    assert(p != NULL);

    if ((err = pthread_cond_signal(&p->condvar)))
        return fail(err);

    return success;
}

status_t wlock_broadcast(wlock p)
{
    int err;

    assert(p != NULL);

    if ((err = pthread_cond_broadcast(&p->condvar)))
        return fail(err);

    return success;
}

status_t wlock_wait(wlock p)
{
    int err;

    assert(p != NULL);

    /* wait for someone to signal us */
    if ((err = pthread_cond_wait(&p->condvar, &p->lock)))
        return fail(err);

    return success;
}

#ifdef CHECK_WLOCK

static void *waiter(void *parg)
{
    wlock w = parg;
    if (!wlock_lock(w))
        abort();

    if (!wlock_wait(w))
        abort();

    if (!wlock_unlock(w))
        abort();

    return NULL;
}

static void *signaler(void *parg)
{
    wlock w = parg;

    if (!wlock_lock(w))
        abort();

    if (!wlock_unlock(w))
        abort();

    if (!wlock_signal(w))
        abort();

    return NULL;
}

/*
 * Now, how do we test a waitable lock?
 * We need at least two threads, one that waits and one that signals.
 * We also need a lock, of course.
 *
 * What about broadcasting? All we need is one signaler and multiple waiters.
 * We can do that, no sweat.
 */

int main(void)
{
    wlock w = wlock_new();
    pthread_t wid, sid;

    pthread_create(&wid, NULL, waiter, w);
    sleep(1); /* Give the thread some time to call wlock_wait() before we signal */
    pthread_create(&sid, NULL, signaler, w);
    pthread_join(wid, NULL);
    pthread_join(sid, NULL);

    wlock_free(w);
    return 0;
}
#endif
