/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include <meta_slotbuf.h>

struct slotbuf_tag {
    void** data;
    size_t size;
    int can_overwrite;
    pthread_mutex_t lock;
    dtor pfn;
};

slotbuf slotbuf_new(size_t size, int can_overwrite, dtor pfn)
{
    slotbuf new;

    assert(size > 0);

    if ((new = malloc(sizeof *new)) == NULL)
        return NULL;

    if ((new->data = calloc(size, sizeof *new->data)) == NULL) {
        free(new);
        return NULL;
    }

    new->can_overwrite = can_overwrite;
    new->size = size;
    pthread_mutex_init(&new->lock, NULL);
    new->pfn = pfn;

    return new;
}

void slotbuf_free(slotbuf this)
{
    size_t i;

    if (this == NULL)
        return;

    for (i = 0; i < this->size; i++) {
        if (this->data[i] != NULL && this->pfn != NULL)
            this->pfn(this->data[i]);
    }

    free(this->data);
    pthread_mutex_destroy(&this->lock);
    free(this);
}

status_t slotbuf_set(slotbuf this, size_t i, void *value)
{
    size_t idx;
    assert(this != NULL);

    idx = i % this->size;
    if (this->data[idx] != NULL) {
        if (!this->can_overwrite)
            return failure;

        if (this->pfn != NULL)
            this->pfn(this->data[idx]);
    }

    this->data[idx] = value;
    return success;
}

void *slotbuf_get(slotbuf this, size_t i)
{
    size_t idx;
    void *data;

    assert(this != NULL);

    idx = i % this->size;
    data = this->data[idx];
    this->data[idx] = NULL;
    return data;
}

size_t slotbuf_nelem(slotbuf this)
{
    size_t i, n;

    assert(this != NULL);

    for (i = n = 0; i < this->size; i++)
        if (this->data[i] != NULL)
            n++;

    return n;
}

void *slotbuf_peek(slotbuf this, size_t i)
{
    size_t idx;
    assert(this != NULL);

    idx = i % this->size;
    return this->data[idx];
}

bool slotbuf_has_data(slotbuf this, size_t i)
{
    size_t idx;
    assert(this != NULL);

    idx = i % this->size;
    return this->data[idx] != NULL;
}

void slotbuf_lock(slotbuf this)
{
    assert(this != NULL);
    if (pthread_mutex_lock(&this->lock))
        abort();
}

void slotbuf_unlock(slotbuf this)
{
    assert(this != NULL);
    if (pthread_mutex_unlock(&this->lock))
        abort();
}

#ifdef CHECK_SLOTBUF

int main(void)
{
    size_t i, n = 1024;
    slotbuf p = slotbuf_new(10, 1, 0);
    void *v;

    for (i = 0; i < n; i++) {
        if (!slotbuf_set(p, i, (void*)i))
            return 1;

        v = slotbuf_get(p, i);
        if (v != (void*)i)
            return 1;
    }

    slotbuf_free(p);
    return 0;
}
#endif
