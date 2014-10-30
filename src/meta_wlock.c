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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>

#include <meta_common.h>
#include <meta_wlock.h>

#ifdef CHECK_WLOCK
#define DEBUG_ME
#endif

/**
 * Implementation of the meta_wlock ADT.
 */
struct wlock_tag {
	pthread_mutexattr_t ma;
	pthread_mutex_t lock;
	pthread_cond_t condvar;

#ifdef DEBUG_ME
	/* Double check that we don't lock ourselves */
	pthread_mutex_t debuglock; /* To avoid having helgrind report errors */
	long locked_by;
	int locked;
#endif

};

wlock wlock_new(void)
{
	wlock p;

	if ((p = malloc(sizeof *p)) != NULL) {
		pthread_mutexattr_init(&p->ma);
		pthread_mutexattr_settype(&p->ma, PTHREAD_MUTEX_ERRORCHECK);
		pthread_mutex_init(&p->lock, &p->ma);
		pthread_cond_init(&p->condvar, NULL);
#ifdef DEBUG_ME
		p->locked_by = -1;
		p->locked = 0;
		pthread_mutex_init(&p->debuglock, NULL);
#endif

	}

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

int wlock_lock(wlock p)
{
	int err;

	assert(p != NULL);
#ifdef DEBUG_ME
	pthread_mutex_lock(&p->debuglock);
	if (p->locked && pthread_equal(pthread_self(), p->locked_by)) {
		fprintf(stderr, "Recursive lock detected, tid==%ld\n",
			(long)pthread_self());
	}
	pthread_mutex_unlock(&p->debuglock);
#endif

	if ((err = pthread_mutex_lock(&p->lock))) {
		errno = err;
		return 0;
	}

#ifdef DEBUG_ME
	pthread_mutex_lock(&p->debuglock);
	p->locked = 1;
	p->locked_by = pthread_self();
	pthread_mutex_unlock(&p->debuglock);
#endif

	return 1;
}

int wlock_unlock(wlock p)
{
	int err;

	assert(p != NULL);
#ifdef DEBUG_ME
	pthread_mutex_lock(&p->debuglock);
	fprintf(stderr, "Unlocking\n");
	if (!p->locked) {
		fprintf(stderr, "Thread %lu tried to unlock an unlocked lock\n",
			pthread_self());
	}
	else if(!pthread_equal(pthread_self(), p->locked_by)) {
		fprintf(stderr, "Mutex locked by someone else(%ld), we're %ld\n",
			(long)p->locked_by, (long)pthread_self());
	}
	pthread_mutex_unlock(&p->debuglock);
#endif

	if ((err = pthread_mutex_unlock(&p->lock))) {
		errno = err;
		return 0;
	}


#ifdef DEBUG_ME
	pthread_mutex_lock(&p->debuglock);
	p->locked = 0;
	p->locked_by = -2;
	pthread_mutex_unlock(&p->debuglock);
#endif

	return 1;
}

int wlock_signal(wlock p)
{
	int err;

	assert(p != NULL);

	if ((err = pthread_cond_signal(&p->condvar))) {
		errno = err;
		return 0;
	}

	return 1;
}

int wlock_broadcast(wlock p)
{
	int err;

	assert(p != NULL);

	if ((err = pthread_cond_broadcast(&p->condvar))) {
		errno = err;
		return 0;
	}
	return 1;
}

int wlock_wait(wlock p)
{
	int err;

	assert(p != NULL);
#ifdef DEBUG_ME
	pthread_mutex_lock(&p->debuglock);
	fprintf(stderr, "Waiting\n");
	assert(p->locked && "Crap, mutex is not locked");
	assert(pthread_equal(pthread_self(), p->locked_by) && "Not locked by this thread");
	pthread_mutex_unlock(&p->debuglock);
#endif

	/* wait for someone to signal us */
	if ((err = pthread_cond_wait(&p->condvar, &p->lock))) {
		errno = err;
		return 0;
	}

#ifdef DEBUG_ME
	p->locked = 1;
	p->locked_by = pthread_self();
#endif
	return 1;
}

#ifdef CHECK_WLOCK

#define DEBUG_ME

static void* waiter(void *parg)
{
	wlock w = parg;
	wlock_lock(w);
	wlock_wait(w);
	wlock_unlock(w);
	return NULL;
}

static void* signaler(void *parg)
{
	wlock w = parg;

	wlock_lock(w);
	wlock_unlock(w);
	wlock_signal(w);
	return NULL;
}

/*
 * Now, how do we test a waitable lock?
 * We need at least two threads, one that waits and one that signals.
 * We also need a lock, of course.
 *
 * What about broadcasting? All we need is one signaler and multiple waiters.
 * We can do that, no sweat.
 *
 * The *real* question is: For how long will we keep all the DEBUG_ME code here?
 * It is quite ugly.
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

