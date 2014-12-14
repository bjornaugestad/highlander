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

#ifdef WITH_VALGRIND
#include <valgrind/memcheck.h>
#endif

#include <stdlib.h>
#include <assert.h>

#ifdef __sun
#	define _POSIX_PTHREAD_SEMANTICS
#endif

#include <pthread.h>

#include <meta_pool.h>

/*
 * The implementation of the pool adt. We allocate room for
 * a set of void *pointers, where each pointer points to one
 * element in the pool. We use a mutex to control access to the pool.
 * The in_use member is 1 if the resource is used by someone, else 0.
 * We allocate one char for each pool member.
 *
 * Changes:
 * 2005-12-15: Removed the in_use member and just zero out the
 * pointer when the resource is in use. pool_recycle() will add
 * a returned resource in the first free slot.
 */
struct pool_tag {
	void** pdata;			/* Array of void *pointers */
	size_t size;			/* Size of the pool */
	size_t nelem;			/* Number of elements added to the pool */
	pthread_mutex_t mutex;	/* access control */
};

pool pool_new(size_t size)
{
	pool new;

	assert(size > 0); /* No point in zero-sized pools */

	if ((new = calloc(1, sizeof *new)) == NULL)
		return NULL;

	if ((new->pdata = calloc(size, sizeof *new->pdata)) == NULL) {
		free(new);
		return NULL;
	}

	pthread_mutex_init(&new->mutex, NULL);
	new->size = size;
	new->nelem = 0;

	return new;
}

void pool_free(pool this, dtor free_fn)
{
	size_t i;


	if (this == NULL)
		return;
		
	/* Free entries if we have a dtor and the entry is not NULL */
	if (free_fn != NULL) {
		assert(this->pdata != NULL);
		assert(this->size > 0);

		for (i = 0; i < this->nelem; i++)
			if (this->pdata[i] != NULL)
				free_fn(this->pdata[i]);
	}

	free(this->pdata);
	pthread_mutex_destroy(&this->mutex);
	free(this);
}

void pool_add(pool this, void *resource)
{
	assert(NULL != this);
	assert(NULL != resource);
	assert(this->nelem < this->size);

	pthread_mutex_lock(&this->mutex);
	this->pdata[this->nelem++] = resource;
	pthread_mutex_unlock(&this->mutex);
}

void *pool_get(pool this)
{
	size_t i;
	void *resource = NULL;
	int error = 0;

	assert(NULL != this);

	error = pthread_mutex_lock(&this->mutex);
	assert(!error);

	/* Find a free resource */
	for (i = 0; i < this->nelem; i++) {
		if (this->pdata[i] != NULL) {
			resource = this->pdata[i];
			this->pdata[i] = NULL;

#ifdef WITH_VALGRIND
			VALGRIND_MAKE_MEM_UNDEFINED(resource, this->size);
#endif
			break;
		}
	}

	error = pthread_mutex_unlock(&this->mutex);
	assert(!error);

	/* It is not legal to return NULL, we must always
	 * have enough resources. */
	assert(i != this->nelem);
	assert(NULL != resource);

	return resource;
}

void pool_recycle(pool this, void *resource)
{
	size_t i;
	int error = 0;

	assert(NULL != this);
	assert(NULL != resource);

	error = pthread_mutex_lock(&this->mutex);
	assert(!error);

	for (i = 0; i < this->nelem; i++) {
		if (this->pdata[i] == NULL) {
			this->pdata[i] = resource;
			break;
		}
	}

	/* If the resource wasnt' released, someone released more objects
	 * than they got. This is something we discourage by asserting. :-) */
	assert(i < this->nelem);

	error = pthread_mutex_unlock(&this->mutex);
	assert(!error);
}

#ifdef CHECK_POOL

#include <stdio.h>

/* Create two threads. Each thread accesses the pool
 * in random ways for some time.
 */
#define NELEM 10000
#define NITER 1000

static void *tfn(void *arg)
{
	pool pool = arg;
	size_t i, niter = NITER;
	void *dummy;

	for (i = 0; i < niter; i++) {
		if ((dummy = pool_get(pool)) == NULL) {
			fprintf(stderr, "Unable to get resource\n");
			exit(77);
		}

		pool_recycle(pool, dummy);
	}

	return NULL;
}


int main(void)
{
	pool pool;
	pthread_t t1, t2;
	size_t i;

	if ((pool = pool_new(NELEM)) == NULL)
		return 77;

	/* Add some items to the pool */
	for (i = 0; i < NELEM; i++) {
		void *dummy = (void*)(i + 1);
		pool_add(pool, dummy);
	}

	/* Start the threads */
	pthread_create(&t1, NULL, tfn, pool);
	pthread_create(&t2, NULL, tfn, pool);

	/* Wait for the threads to finish */
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	pool_free(pool, NULL);
	return 0;
}
#endif
