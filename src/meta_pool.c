/*
    libcoremeta - A library of useful C functions and ADT's
    Copyright (C) 2000-2006 B. Augestad, bjorn.augestad@gmail.com 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <stdlib.h>
#include <assert.h>

#ifdef __sun
#	define _POSIX_PTHREAD_SEMANTICS
#endif

#include <pthread.h>

#include <meta_pool.h>

/**
 * The implementation of the pool adt. We allocate room for
 * a set of void* pointers, where each pointer points to one 
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
	void** pdata;			/* Array of void* pointers */
	size_t size;			/* Size of the pool */
	size_t nelem;			/* Number of elements added to the pool */
	pthread_mutex_t mutex;	/* access control */
};

pool pool_new(size_t size)
{
	pool p;
	
	assert(size > 0); /* No point in zero-sized pools */

	if( (p = mem_calloc(1, sizeof *p)) == NULL)
		;
	else if( (p->pdata = mem_calloc(size, sizeof *p->pdata)) == NULL) {
		mem_free(p);
		p = NULL;
	}
	else {
		pthread_mutex_init(&p->mutex, NULL); 
		p->size = size;
		p->nelem = 0;
	}

	return p;
}

void pool_free(pool p, dtor free_fn) 
{
	size_t i;


	if(p != NULL) {
		/* Free entries if we have a dtor and the entry is not NULL */
		if(free_fn != NULL) {
			assert(p->pdata != NULL);
			assert(p->size > 0);

			for(i = 0; i < p->nelem; i++) 
				if(p->pdata[i] != NULL)
					free_fn(p->pdata[i]);
		}

		mem_free(p->pdata);
		pthread_mutex_destroy(&p->mutex);
		mem_free(p);
	}
}

void pool_add(pool p, void* resource)
{
	assert(NULL != p);
	assert(NULL != resource);
	assert(p->nelem < p->size);

	pthread_mutex_lock(&p->mutex);
	p->pdata[p->nelem++] = resource;
	pthread_mutex_unlock(&p->mutex);
}

void* pool_get(pool p)
{
	size_t i;
	void* resource = NULL;
	int error = 0;

	assert(NULL != p);

	error = pthread_mutex_lock(&p->mutex);
	assert(!error);

	/* Find a free resource */
	for(i = 0; i < p->nelem; i++) {
		if(p->pdata[i] != NULL) {
			resource = p->pdata[i];
			p->pdata[i] = NULL;
			break;
		}
	}

	error = pthread_mutex_unlock(&p->mutex);
	assert(!error);

	/* It is not legal to return NULL, we must always 
	 * have enough resources.
	 */
	assert(i != p->nelem);
	assert(NULL != resource);
	if(resource == NULL) /* Release version paranoia */
		abort();

	return resource;
}

void pool_recycle(pool p, void* resource)
{
	size_t i;
	int error = 0;

	assert(NULL != p);
	assert(NULL != resource);

	error = pthread_mutex_lock(&p->mutex);
	assert(!error);

	for(i = 0; i < p->nelem; i++) {
		if(p->pdata[i] == NULL) {
			p->pdata[i] = resource;
			break;
		}
	}

	/* If the resource wasnt' released, someone released more objects
	 * than they got. This is something we discourage by asserting. :-)
	 */
	assert(i < p->nelem);

	error = pthread_mutex_unlock(&p->mutex);
	assert(!error);
}

#ifdef CHECK_POOL

#include <stdio.h>

/* Create two threads. Each thread accesses the pool
 * in random ways for some time. 
 */
#define NELEM 10000
#define NITER 1000

static void* tfn(void* arg)
{
	pool p;
	size_t i, niter = NITER;
	void* dummy;
	
	p = arg;

	for(i = 0; i < niter; i++) {
		if( (dummy = pool_get(p)) == NULL) {
			fprintf(stderr, "Unable to get resource\n");
			exit(77);
		}
		pool_recycle(p, dummy);
	}

	return NULL;
}


int main(void)
{
	pool p;
	pthread_t t1, t2;
	size_t i;

	if( (p = pool_new(NELEM)) == NULL)
		return 77;

	/* Add some items to the pool */
	for(i = 0; i < NELEM; i++) {
		void* dummy = (void*)(i + 1);
		pool_add(p, dummy);
	}

	/* Start the threads */
	pthread_create(&t1, NULL, tfn, p);
	pthread_create(&t2, NULL, tfn, p);

	/* Wait for the threads to finish */
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	pool_free(p, NULL);
	return 0;
}
#endif
