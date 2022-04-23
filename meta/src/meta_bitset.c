/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <meta_common.h>
#include <meta_bitset.h>

bitset bitset_new(size_t bitcount)
{
    bitset b;
    size_t size = bitcount / CHAR_BIT + (bitcount % CHAR_BIT ? 1 : 0);

    if ((b = calloc(1, sizeof *b)) == NULL)
        return NULL;

    if ((b->data = malloc(size)) == NULL) {
        free(b);
        return NULL;
    }

    b->size = size;
    b->bitcount = bitcount;
    bitset_clear_all(b);

    return b;
}

void bitset_free(bitset this)
{
    if (this) {
        free(this->data);
        free(this);
    }
}

bitset bitset_map(void *data, size_t elemcount)
{
    bitset new;

    assert(data != NULL);

    if ((new = calloc(1, sizeof *new)) != NULL) {
        new->size = elemcount;
        new->data = data;
    }

    return new;
}

void bitset_unmap(bitset this)
{
    free(this);
}

int bitset_allzero(const bitset this)
{
    size_t i;

    assert(this != NULL);

    for (i = 0; i < this->size; i++)
        if (this->data[i] != 0)
            return 0;

    return 1;
}

int bitset_allone(const bitset this)
{
    size_t i;

    assert(this != NULL);

    for (i = 0; i < this->size; i++)
        if (this->data[i] != 0xff)
            return 0;

    return 1;
}

void bitset_remap(bitset this, void *mem, size_t cb)
{
    assert(this != NULL);
    assert(mem != NULL);
    assert(cb > 0);

    free(this->data);
    this->data = mem;
    this->size = cb;
}

bitset bitset_and(bitset b, bitset c)
{
    size_t i, nbits, size;
    bitset result;
    unsigned char *sa, *sb, *sc;

    assert(b != NULL);
    assert(c != NULL);
    assert(bitset_size(b) == bitset_size(c));

    nbits = bitset_bitcount(b);
    if ((result = bitset_new(nbits)) == NULL)
        return NULL;

    sa = bitset_data(result);
    sb = bitset_data(b);
    sc = bitset_data(c);

    size = bitset_size(b);
    for (i = 0; i < size; i++)
        *sa++ = *sb++ & *sc++;

    return result;
}

bitset bitset_or(bitset b, bitset c)
{
    size_t i, nbits, size;
    bitset result;
    unsigned char *sa, *sb, *sc;

    assert(b != NULL);
    assert(c != NULL);
    assert(bitset_size(b) == bitset_size(c));

    nbits = bitset_bitcount(b);
    if ((result = bitset_new(nbits)) == NULL)
        return NULL;

    sa = bitset_data(result);
    sb = bitset_data(b);
    sc = bitset_data(c);

    size = bitset_size(b);
    for (i = 0; i < size; i++)
        *sa++ = *sb++ | *sc++;

    return result;
}

bitset bitset_xor(bitset b, bitset c)
{
    size_t i, nbits, size;
    bitset result;
    unsigned char *sa, *sb, *sc;

    assert(b != NULL);
    assert(c != NULL);
    assert(bitset_size(b) == bitset_size(c));

    nbits = bitset_bitcount(b);
    if ((result = bitset_new(nbits)) == NULL)
        return NULL;

    sa = bitset_data(result);
    sb = bitset_data(b);
    sc = bitset_data(c);

    size = bitset_size(b);
    for (i = 0; i < size; i++)
        *sa++ = *sb++ ^ *sc++;

    return result;
}

#ifdef BITSET_CHECK


int main(void)
{
    bitset a, b, c;
    size_t i, nelem = 10000;

    if ((b = bitset_new(nelem)) == NULL)
        return 1;

    for (i = 0; i < nelem; i++)
        bitset_set(b, i);

    for (i = 0; i < nelem; i++)
        if (!bitset_is_set(b, i))
            return 1;

    for (i = 0; i < nelem; i++)
        bitset_clear(b, i);

    for (i = 0; i < nelem; i++)
        if (bitset_is_set(b, i))
            return 1;

    // Now test the binary ops.
    if ((c = bitset_dup(b)) == NULL)
        return 1;

    // Since both b and c are all-zero, b & c == b | c == all-zero
    // It's an error if a has 1..n bit high.
    if ((a = bitset_and(b, c)) == NULL)
        return 1;

    if (!bitset_allzero(a))
        return 1;

    bitset_free(a);

    // Now do the same for _or().
    if ((a = bitset_or(b, c)) == NULL)
        return 1;

    if (!bitset_allzero(a))
        return 1;

    bitset_free(a);

    // Now do the same for _xor().
    if ((a = bitset_xor(b, c)) == NULL)
        return 1;

    if (!bitset_allzero(a))
        return 1;

    bitset_free(a);

    // Now set every second bit in b and c. Even bits in b and odd
    // bits in c. Then repeat all the tests.
    for (i = 0; i < nelem; i++) {
        if (i % 2 == 0)
            bitset_set(b, i);
        else
            bitset_set(c, i);
    }

    // Since no bits in b and c match, a should be all zero.
    if ((a = bitset_and(b, c)) == NULL)
        return 1;

    if (!bitset_allzero(a))
        return 1;

    bitset_free(a);

    // All bits should be high, since b and c has the patterns 101010 and 010101
    if ((a = bitset_or(b, c)) == NULL)
        return 1;

    if (!bitset_allone(a))
        return 1;

    bitset_free(a);

    // Now do the same for _xor(). 1 ^ 0 == 1.
    if ((a = bitset_xor(b, c)) == NULL)
        return 1;

    if (!bitset_allone(a))
        return 1;

    bitset_free(a);

    // b and c still have the 1010 and 0101 patterns. Let's use
    // them to check the new _eq() functions.
    bitset_and_eq(b, c); // b Now is all zero, hopefully
    if (!bitset_allzero(b))
        return 1;

    // If we now OR b and c, then b should equal c.
    bitset_or_eq(b, c);
    if (bitset_allzero(b) || bitset_allone(b))
        return 1;
    if (bitset_cmp(b, c) != 0)
        return 1;

    // Right, if we now XOR b and c, then b will be all zero.
    bitset_xor_eq(b, c);
    if (!bitset_allzero(b))
        return 1;

    // Now test the binary ops.
    bitset_free(c);
    if ((c = bitset_dup(b)) == NULL)
        return 77;

    // Since both b and c are all-zero, b & c == b | c == all-zero
    // It's an error if a has 1..n bit high.
    if ((a = bitset_and(b, c)) == NULL)
        return 77;

    if (!bitset_allzero(a))
        return 77;

    bitset_free(a);

    // Now do the same for _or().
    if ((a = bitset_or(b, c)) == NULL)
        return 77;

    if (!bitset_allzero(a))
        return 77;

    bitset_free(a);

    // Now do the same for _xor().
    if ((a = bitset_xor(b, c)) == NULL)
        return 77;

    if (!bitset_allzero(a))
        return 77;

    bitset_free(a);

    // Now set every second bit in b and c. Even bits in b and odd
    // bits in c. Then repeat all the tests.
    for (i = 0; i < nelem; i++) {
        if (i % 2 == 0)
            bitset_set(b, i);
        else
            bitset_set(c, i);
    }

    // Since no bits in b and c match, a should be all zero.
    if ((a = bitset_and(b, c)) == NULL)
        return 77;

    if (!bitset_allzero(a))
        return 77;

    bitset_free(a);

    // All bits should be high, since b and c has the patterns 101010 and 010101
    if ((a = bitset_or(b, c)) == NULL)
        return 77;

    if (!bitset_allone(a))
        return 77;

    bitset_free(a);

    // Now do the same for _xor(). 1 ^ 0 == 1.
    if ((a = bitset_xor(b, c)) == NULL)
        return 77;

    if (!bitset_allone(a))
        return 77;

    bitset_free(a);

    // b and c still has the 1010 and 0101 patterns. Let's use
    // them to check the new _eq() functions.
    bitset_and_eq(b, c); // b Now is all zero, hopefully
    if (!bitset_allzero(b))
        return 77;

    // If we now OR b and c, then b should equal c.
    bitset_or_eq(b, c);
    if (bitset_allzero(b) || bitset_allone(b))
        return 77;
    if (bitset_cmp(b, c) != 0)
        return 77;

    // Right, if we now XOR b and c, then b will be all zero.
    bitset_xor_eq(b, c);
    if (!bitset_allzero(b))
        return 77;

    bitset_free(b);
    bitset_free(c);
    return 0;
}

#endif
