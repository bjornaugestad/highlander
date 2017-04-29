/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_SLOTBUF_H
#define META_SLOTBUF_H

#include <stdbool.h>
#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct slotbuf_tag *slotbuf;

slotbuf slotbuf_new(size_t size, int can_overwrite, dtor fn) 
    __attribute__((malloc));

void slotbuf_free(slotbuf p);

status_t slotbuf_set(slotbuf p, size_t i, void *value)
    __attribute__((nonnull(1, 3)))
    __attribute__((warn_unused_result));

void *slotbuf_get(slotbuf p, size_t i)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

void *slotbuf_peek(slotbuf p, size_t i)
    __attribute__((nonnull(1)));

bool slotbuf_has_data(slotbuf p, size_t i)
    __attribute__((nonnull(1)));

size_t slotbuf_nelem(slotbuf p)
    __attribute__((nonnull(1)));

void slotbuf_lock(slotbuf p)
    __attribute__((nonnull(1)));

void slotbuf_unlock(slotbuf p)
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
