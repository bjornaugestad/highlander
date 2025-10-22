/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#include <meta_pool.h>

/*
 * The implementation of the pool adt. We allocate room for
 * a set of void *pointers, where each pointer points to one
 * element in the pool. We use a mutex to control access to the pool.
 */
struct pool_tag {
    void** pdata;			/* Array of void *pointers */
    size_t size;			/* Size of the pool */
    size_t nelem;			/* Number of elements added to the pool */
    pthread_mutex_t mutex;	/* access control */
};

pool pool_new(size_t size)
{
    assert(size > 0); /* No point in zero-sized pools */

    pool new = calloc(1, sizeof *new);
    if (new == NULL)
        return NULL;

    new->pdata = calloc(size, sizeof *new->pdata);
    if (new->pdata == NULL) {
        free(new);
        return NULL;
    }

    pthread_mutex_init(&new->mutex, NULL);
    new->size = size;
    new->nelem = 0;

    return new;
}

// Free entries if we have a dtor and the entry is not NULL
void pool_free(pool this, dtor dtorfn)
{
    if (this == NULL)
        return;

    if (dtorfn != NULL) {
        int error = pthread_mutex_lock(&this->mutex);
        if (error)
            die("WTF?\n");

        for (size_t i = 0; i < this->nelem; i++)
            if (this->pdata[i] != NULL)
                dtorfn(this->pdata[i]);

        error = pthread_mutex_unlock(&this->mutex);
        if (error)
            die("WTF?\n");
    }

    free(this->pdata);
    pthread_mutex_destroy(&this->mutex);
    free(this);
}

status_t pool_add(pool this, void *resource)
{
    assert(this != NULL);
    assert(resource != NULL);
    assert(this->nelem < this->size);

    int error = pthread_mutex_lock(&this->mutex);
    if (error)
        return fail(error);

    this->pdata[this->nelem++] = resource;
    error = pthread_mutex_unlock(&this->mutex);
    if (error)
        return fail(error);

    return success;
}

status_t pool_get(pool this, void **ppres)
{
    assert(this != NULL);

    int error = pthread_mutex_lock(&this->mutex);
    if (error)
        return fail(error);

    // Find a free resource 
    size_t i, nelem = this->nelem; // avoid race conds and holding mutex for too long
    void *resource = NULL;
    for (i = 0; i < nelem; i++) {
        if (this->pdata[i] != NULL) {
            resource = this->pdata[i];
            this->pdata[i] = NULL;
            break;
        }
    }

    error = pthread_mutex_unlock(&this->mutex);
    if (error)
        return fail(error);

    if (i == nelem)
        return fail(ENOSPC);

    if (resource == NULL)
        return fail(EINVAL);

    *ppres = resource;
    return success;
}

status_t pool_recycle(pool this, void *resource)
{
    assert(this != NULL);
    assert(resource != NULL);

    int error = pthread_mutex_lock(&this->mutex);
    if (error)
        return fail(error);

    size_t i, nelem = this->nelem;
    for (i = 0; i < nelem; i++) {
        if (this->pdata[i] == NULL) {
            this->pdata[i] = resource;
            break;
        }
    }

    error = pthread_mutex_unlock(&this->mutex);
    if (error)
        return fail(error);

    // If the resource wasnt' released, someone released more objects than they got
    if (i == nelem) {
        die("WTF?\n");
        return fail(ENOENT);
    }

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
    pool p = arg;
    size_t i, niter = NITER;
    void *dummy;

    for (i = 0; i < niter; i++) {
        if (!pool_get(p, (void **)&dummy)) {
            fprintf(stderr, "Unable to get resource\n");
            exit(77);
        }

        if (!pool_recycle(p, dummy))
            die("internal error\n");
    }

    return NULL;
}

int main(void)
{
    pool p;
    pthread_t t1, t2;
    size_t i;

    if ((p = pool_new(NELEM)) == NULL)
        return 77;

    /* Add some items to the pool */
    for (i = 0; i < NELEM; i++) {
        void *dummy = (void*)(i + 1);
        if (!pool_add(p, dummy))
            die("Could not add object to pool\n");
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
