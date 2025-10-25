/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_BITSET_H
#define META_BITSET_H

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bitset_tag* bitset;

// Implementation of the bitset ADT.
// The bits are stored in an array of unsigned char
// TODO: Consider using bigger units, e.g. uint64_t for 
// much faster processing. We store bitcount, so it should be
// easy to use a bigger unit.
struct bitset_tag {
    size_t bitcount, size;
    unsigned char *data;
};


bitset bitset_new(size_t bitcount) __attribute__((malloc, warn_unused_result));

void bitset_free(bitset b);

static inline __attribute__((nonnull, warn_unused_result)) size_t bitset_size(bitset this)
{
    assert(this != NULL);
    return this->size;
}

// lhs &= rhs
static inline __attribute__((nonnull)) void bitset_and_eq(bitset lhs, bitset rhs)
{
    size_t i;

    assert(lhs != NULL);
    assert(rhs != NULL);
    assert(bitset_size(lhs) == bitset_size(rhs));

    for (i = 0; i < lhs->size; i++)
        lhs->data[i] &= rhs->data[i];
}

static inline __attribute__((nonnull)) void bitset_or_eq(bitset lhs, bitset rhs)
{
    size_t i;

    assert(lhs != NULL);
    assert(rhs != NULL);
    assert(bitset_size(lhs) == bitset_size(rhs));

    for (i = 0; i < lhs->size; i++)
        lhs->data[i] |= rhs->data[i];
}

static inline __attribute__((nonnull)) void bitset_xor_eq(bitset lhs, bitset rhs)
{
    size_t i;

    assert(lhs != NULL);
    assert(rhs != NULL);
    assert(bitset_size(lhs) == bitset_size(rhs));

    for (i = 0; i < lhs->size; i++)
        lhs->data[i] ^= rhs->data[i];
}

static inline __attribute__((nonnull, warn_unused_result)) int bitset_cmp(const bitset a, const bitset b)
{
    size_t i;

    assert(a != NULL);
    assert(b != NULL);
    assert(bitset_size(a) == bitset_size(b));

    for (i = 0; i < a->size; i++) {
        int delta = a->data[i] - b->data[i];
        if (delta)
            return delta;
    }

    return 0;
}

static inline __attribute__((nonnull, warn_unused_result)) bitset bitset_dup(bitset b)
{
    bitset new;

    assert(b != NULL);

    if ((new = bitset_new(b->bitcount)) == NULL)
        return NULL;

    memcpy(new->data, b->data, new->size);
    return new;
}

static inline __attribute__((nonnull)) void bitset_set(bitset this, size_t i)
{
    assert(this != NULL);
    assert(i < this->size * CHAR_BIT);

    this->data[i / CHAR_BIT] |= (unsigned char)(1u << (i % CHAR_BIT));
}

static inline bool bitset_is_set(const bitset this, size_t i)
{
    assert(this != NULL);
    assert(i < this->size * CHAR_BIT);

    return this->data[i / CHAR_BIT] & (1u << (i % CHAR_BIT));
}

static inline __attribute__((nonnull)) void bitset_clear_all(bitset this)
{
    assert(this != NULL);
    memset(this->data, '\0', this->size);
}

static inline __attribute__((nonnull)) void bitset_set_all(bitset this)
{
    assert(this != NULL);
    memset(this->data, 0xff, this->size);
}

static inline __attribute__((nonnull, warn_unused_result))
size_t bitset_bitcount(bitset this)
{
    assert(this != NULL);
    return this->bitcount;
}

static inline __attribute__((nonnull, warn_unused_result)) 
void *bitset_data(bitset this)
{
    assert(this != NULL);
    return this->data;
}

static inline __attribute__((nonnull)) 
void bitset_clear(bitset this, size_t i)
{
    assert(this != NULL);
    assert(i < this->size * CHAR_BIT);

    this->data[i / CHAR_BIT] &= (unsigned char) ~(1u << (i % CHAR_BIT));
}


int bitset_allzero(const bitset b)          __attribute__((nonnull, warn_unused_result));
int bitset_allone(const bitset b)           __attribute__((nonnull, warn_unused_result));

bitset bitset_map(void *mem, size_t cb) __attribute__((nonnull, warn_unused_result));
void bitset_remap(bitset b, void *mem, size_t cb) __attribute__((nonnull));
void bitset_unmap(bitset b) __attribute__((nonnull));

// equals a = b & c; and returns a. Remember to free it.
bitset bitset_and(bitset b1, bitset b2)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

bitset bitset_or(bitset b1, bitset b2)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

bitset bitset_xor(bitset b1, bitset b2)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

// equals a &= b;
void bitset_and_eq(bitset a, bitset b) __attribute__((nonnull));
void bitset_or_eq (bitset a, bitset b) __attribute__((nonnull));
void bitset_xor_eq(bitset a, bitset b) __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
