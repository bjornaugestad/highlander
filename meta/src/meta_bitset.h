/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_BITSET_H
#define META_BITSET_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bitset_tag* bitset;

bitset bitset_new(size_t bitcount) __attribute__((malloc, warn_unused_result));
bitset bitset_dup(bitset b) __attribute__((malloc, warn_unused_result));

void bitset_free(bitset b);

int bitset_cmp(const bitset a, const bitset b)
    __attribute__((nonnull, warn_unused_result));

void bitset_set(bitset b, size_t i)   __attribute__((nonnull));
void bitset_set_all(bitset b)         __attribute__((nonnull));
void bitset_clear(bitset b, size_t i) __attribute__((nonnull));
void bitset_clear_all(bitset b)       __attribute__((nonnull));

int bitset_is_set(const bitset b, size_t i) __attribute__((nonnull, warn_unused_result));
int bitset_allzero(const bitset b)          __attribute__((nonnull, warn_unused_result));
int bitset_allone(const bitset b)           __attribute__((nonnull, warn_unused_result));

size_t bitset_size(bitset b)     __attribute__((nonnull, warn_unused_result));
size_t bitset_bitcount(bitset b) __attribute__((nonnull, warn_unused_result));

bitset bitset_map(void *mem, size_t cb) __attribute__((nonnull, warn_unused_result));
void bitset_remap(bitset b, void *mem, size_t cb) __attribute__((nonnull));
void bitset_unmap(bitset b) __attribute__((nonnull));
void *bitset_data(bitset b) __attribute__((nonnull, warn_unused_result));

// equals a = b & c; and returns a. Remember to free it.
bitset bitset_and(bitset b1, bitset b2)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

bitset bitset_or(bitset b1, bitset b2)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

bitset bitset_xor(bitset b1, bitset b2)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

// equals a &= b;
void bitset_and_eq(bitset a, bitset b) __attribute__((nonnull));
void bitset_or_eq (bitset a, bitset b) __attribute__((nonnull));
void bitset_xor_eq(bitset a, bitset b) __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
