/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
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
    assert(this != NULL);
    assert(resource != NULL);
    assert(this->nelem < this->size);

    pthread_mutex_lock(&this->mutex);
    this->pdata[this->nelem++] = resource;
    pthread_mutex_unlock(&this->mutex);
}

status_t pool_get(pool this, void **ppres)
{
    size_t i;
    void *resource = NULL;
    int error = 0;

    assert(this != NULL);

    error = pthread_mutex_lock(&this->mutex);
    if (error)
        return fail(error);

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
    if (error)
        return fail(error);

    if (i == this->nelem)
        return fail(ENOSPC);

    if (resource == NULL)
        return fail(EINVAL);

    *ppres = resource;
    return success;
}

status_t pool_recycle(pool this, void *resource)
{
    size_t i;
    int error = 0;

    assert(this != NULL);
    assert(resource != NULL);

    error = pthread_mutex_lock(&this->mutex);
    if (error)
        return fail(error);

    for (i = 0; i < this->nelem; i++) {
        if (this->pdata[i] == NULL) {
            this->pdata[i] = resource;
            break;
        }
    }

    error = pthread_mutex_unlock(&this->mutex);
    if (error)
        return fail(error);

    /* If the resource wasnt' released, someone released more objects
     * than they got. */
    if (i == this->nelem)
        return fail(ENOENT);

    return success;
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
        if (!pool_get(pool, (void **)&dummy)) {
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
