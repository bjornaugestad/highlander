/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_SLOTBUF_H
#define META_SLOTBUF_H

/*
 * What's this slotbuf thing anyway? A slot buffer is
 * a set of slots indexed by an integer, but the index is
 * adjusted to fit.
 *
 *     actual_index = index % nslots
 *
 * This means that we can use slotbufs for many things, like
 * an array indexed by an increasing value.
 *
 * Not sure how usable this ADT is. Maybe archive it?
 */

#include <stdbool.h>
#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct slotbuf_tag *slotbuf;

slotbuf slotbuf_new(size_t size, int can_overwrite, dtor fn)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void slotbuf_free(slotbuf p);

status_t slotbuf_set(slotbuf p, size_t i, void *value)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

void *slotbuf_get(slotbuf p, size_t i)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

void *slotbuf_peek(slotbuf p, size_t i)
    __attribute__((nonnull));

bool slotbuf_has_data(slotbuf p, size_t i)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

size_t slotbuf_nelem(slotbuf p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

void slotbuf_lock(slotbuf p)
    __attribute__((nonnull));

void slotbuf_unlock(slotbuf p)
    __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
