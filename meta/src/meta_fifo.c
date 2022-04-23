/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <meta_common.h>
#include <meta_fifo.h>
#include <meta_wlock.h>

/*
 * Implementation of the fifo. We store all data in an array of void *pointers.
 * A slot is empty if the void *is NULL. We also keep track of the first and
 * last entry in the fifo.
 *
 * The indexes work like this:
 * a) iread and iwrite always point to the next item to read from or write to
 * b) Even so, that does not mean that an item is readable.
 * c) if iread == iwrite, you can write to iwrite, but not read from iread
 *	  since iwrite has not been written to yet.
 * d) if iread != iwrite, you can read from iread.
 * e) iwrite is not writeable if pelem[iwrite ] != NULL.
 */
struct fifo_tag {
    size_t size;
    size_t nelem;
    size_t iread;	/* Where to read from */
    size_t iwrite;	/* Where to write to */
    void** pelem;
    wlock lock;
};

fifo fifo_new(size_t size)
{
    fifo p;

    assert(size > 0);

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    if ((p->lock = wlock_new()) == NULL) {
        free(p);
        return NULL;
    }

    if ((p->pelem = calloc(size, sizeof *p->pelem)) == NULL) {
        wlock_free(p->lock);
        free(p);
        return NULL;
    }

    p->size = size;
    p->nelem = 0;
    p->iwrite = p->iread = 0;

    return p;
}

void fifo_free(fifo p, dtor dtor_fn)
{
    if (p == NULL)
        return;

    if (dtor_fn != NULL) {
        size_t i;

        for (i = 0; i < p->size; i++) {
            if (p->pelem[i] != NULL)
                dtor_fn(p->pelem[i]);
        }
    }

    wlock_free(p->lock);
    free(p->pelem);
    free(p);
}

status_t fifo_lock(fifo p)
{
    assert(p != NULL);

    return wlock_lock(p->lock);
}

status_t fifo_unlock(fifo p)
{
    assert(p != NULL);

    return wlock_unlock(p->lock);
}

size_t fifo_nelem(fifo p)
{
    assert(p != NULL);
    return p->nelem;
}

size_t fifo_free_slot_count(fifo p)
{
    assert(p != NULL);
    return p->size - p->nelem;
}

status_t fifo_add(fifo p, void *data)
{
    assert(p != NULL);

    /* Check for wraparounds. */
    if (p->iwrite == p->size)
        p->iwrite = 0;

    /* Do not write if slot is taken already */
    if (p->pelem[p->iwrite] != NULL)
        return failure;

    p->pelem[p->iwrite++] = data;
    p->nelem++;
    return success;
}

void *fifo_peek(fifo p, size_t i)
{
    size_t ipeek;

    assert(p != NULL);
    assert(i < p->nelem);

    /* Check for wraparound */
    ipeek = (p->iread + i) % p->size;
    return p->pelem[ipeek];
}

void *fifo_get(fifo p)
{
    void *data;
    assert(p != NULL);

    /* Check for wraparound */
    if (p->iread == p->size)
        p->iread = 0;

    /* Do not read empty slots */
    if (p->pelem[p->iread] == NULL)
        return NULL;

    data = p->pelem[p->iread];
    p->pelem[p->iread] = NULL;
    p->iread++;
    p->nelem--;

    return data;
}


status_t fifo_write_signal(fifo p, void *data)
{
    status_t rc;

    assert(p != NULL);
    assert(data != NULL);

    if (!fifo_lock(p))
        return failure;

    if (!fifo_add(p, data)) {
        fifo_unlock(p);
        return failure;
    }

    rc = fifo_signal(p);
    fifo_unlock(p);

    return rc;
}

status_t fifo_wait_cond(fifo p)
{
    assert(p != NULL);

    if (!fifo_lock(p))
        return failure;

    if (!wlock_wait(p->lock)) {
        fifo_unlock(p);
        return failure;
    }

    /* OK, we have the lock. See if there are data or not.
     * No data means that fifo_wake() was called.
     */
    if (fifo_nelem(p) == 0) {
        fifo_unlock(p);
        return fail(ENOENT);
    }

    return success;
}

status_t fifo_signal(fifo p)
{
    assert(p != NULL);

    return wlock_signal(p->lock);
}

status_t fifo_wake(fifo p)
{
    assert(p != NULL);

    return wlock_broadcast(p->lock);
}

#ifdef CHECK_FIFO

#include <stdio.h>
#include <pthread.h>

static void *writer(void *arg)
{
    int i, n = 3;
    fifo f = arg;
    char *s;

    for (i = 0; i < n; i++) {
        if ((s = malloc(100)) == NULL)
            return NULL;

        sprintf(s, "writer %d", i);
        if (!fifo_write_signal(f, s)) {
            fprintf(stderr, "fifo_write_signal failed!\n");
            return NULL;
        }

        sleep(1);
    }

    fprintf(stderr, "Exiting %s...\n", __func__);
    return NULL;
}

static void *reader(void *arg)
{
    fifo f = arg;

    while (fifo_wait_cond(f)) {
        char *s;

        while ((s = fifo_get(f)) != NULL) {
            fprintf(stderr, "From reader, who read: %s\n", s);
            free(s);
        }

        if (!fifo_unlock(f))
            break;
    }

    fprintf(stderr, "Exiting %s...\n", __func__);
    return NULL;
}


int main(void)
{
    fifo f;
    size_t i, nelem;
    status_t rc;
    char dummydata[1000] = "Hello";
    clock_t stop, start;
    double duration;


    nelem = 1*1000*1000;
    f = fifo_new(nelem);

#if 1

    /* Fill the fifo completely */
    start = clock();
    for (i = 0; i < nelem; i++) {
        rc = fifo_add(f, dummydata);
        assert(rc != 0);
    }
    stop = clock();
    duration = (stop - start) * 1.0 / CLOCKS_PER_SEC;
    printf("%s: Added %lu elements in %f seconds\n", __FILE__, (unsigned long)nelem, duration);

    assert(fifo_nelem(f) == nelem);

    /* Test fifo_peek() */
    for (i = 0; i < nelem; i++) {
        const char *s = fifo_peek(f, i);
        /* Stupid workaround for gcc 4.0.2 optimization in conjunction with assert() */
        int xi;
        xi = (s != NULL); assert(xi);
        xi = (strcmp(s, dummydata) == 0); assert(xi);
    }

    /* Add a new one, this should fail */
    assert(fifo_add(f, dummydata) == 0);

    /* Now read two and then add one. That should be possible */
    assert(fifo_get(f));
    assert(fifo_get(f));
    assert(fifo_nelem(f) == nelem - 2);
    assert(fifo_add(f, dummydata) != 0);
    assert(fifo_nelem(f) == nelem - 1);

    /* Test fifo_peek() */
    for (i = 0; i < fifo_nelem(f); i++) {
        const char *s = fifo_peek(f, i);
        int xi;
        xi = (s != NULL); assert(xi);
        xi = (strcmp(s, dummydata) == 0); assert(xi);
    }

    /* The first should be OK, the next must fail */
    assert(fifo_add(f, dummydata) != 0);
    assert(fifo_nelem(f) == nelem);

    assert(fifo_add(f, dummydata) == 0);
    assert(fifo_nelem(f) == nelem);

    start = clock();
    for (i = 0; i < nelem; i++) {
        char *x = fifo_get(f);
        (void)x;
    }
    stop = clock();
    duration = (stop - start) * 1.0 / CLOCKS_PER_SEC;
    printf("%s: Got %lu elements in %f seconds\n", __FILE__, (unsigned long)nelem, duration);

#endif

    /* Now check signalling and wait for data.
     * We start two threads, a reader and a writer.
     * The writer writes n times to the fifo and the reader
     * prints the data.
     * This thread joins the writer and calls fifo_wake() when
     * the writer is done, to stop the reader thread in a controlled
     * manner.
     */
     {
         pthread_t w, r;

        pthread_create(&r, NULL, reader, f);
        pthread_create(&w, NULL, writer, f);
        pthread_join(w, NULL);
        if (!fifo_wake(f))
            exit(EXIT_FAILURE);

        pthread_join(r, NULL);
     }

    fifo_free(f, free);
    return 0;
}

#endif
