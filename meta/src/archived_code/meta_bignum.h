#ifndef META_BIGNUM_H
#define META_BIGNUM_H

/*
 * bignums are big numbers, up to 4096 bits wide. We provide
 * support for math operations on them. Not sure what to use
 * them for yet. Maybe for metassl? :)
 *
 * Update 20160614: The original implementation was based on
 * the implementation in "the book". That implementation is
 * too slow. Also, I'm uncomfortable with just copying code
 * from a book without understand each and every detail.
 * Also, in order to speed things up, we need to do arithmetic
 * in something bigger than char. Also, we want to be able
 * to paralellize stuff. Also, we want to utilize GPUs, eventually.
 *
 * Current working hypothesis is that it is much much faster
 * to do n 64-bit ops without all the if/else/realloc stuff.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include <meta_common.h>

// 64 * 64 == 4096. Magic :)
#define META_BIGNUM_MAXBITS 4096

// Max number of bytes. We intentionally do not use CHAR_BIT.
#define META_BIGNUM_SIZE (META_BIGNUM_MAXBITS / (sizeof (uint64_t) * CHAR_BIT))

// We store our values here, in LSB order.
typedef struct {
    uint64_t value[META_BIGNUM_SIZE] __attribute__((aligned(8)));
} bignum;

// Create a new bignum object. Return NULL if length of val mod 8 isn't zero.
// Also, return NULL if length of val exceeds 512.
// val shall contain hexadecimal values.
bignum* bignum_new(const char *val);
void bignum_free(bignum *p);

// Math operations we support. Name should tell you what they do.
// The functions all return success on success, and failure on failure.
// errno will be set when a function can fail for more than one reason.
//
// Keep in mind that, for now, all bignums are unsigned integers.
// No sign involved.
status_t bignum_add(bignum * restrict dest, const bignum *a, const bignum *b);
status_t bignum_sub(bignum * restrict dest, const bignum *a, const bignum *b);
status_t bignum_mul(bignum * restrict dest, const bignum *a, const bignum *b);

status_t bignum_div(bignum * restrict dest, const bignum *a, const bignum *b);
status_t bignum_mod(bignum * restrict dest, const bignum *a, const bignum *b);
status_t bignum_divmod(bignum * restrict quot, bignum * restrict rem, const bignum *a, const bignum *b);

// bitnum_set uses 2 bytes from value for each byte stored in the
// bignum object. This is because the value contains hex numbers.
status_t bignum_set(bignum *p, const char *value)
    __attribute__((warn_unused_result));

// Let users verify their values before calling _set(), as _set() will assert
// if values are illegal. A string is a valid bignum
// if length % 8 == 0, if length < MAX_BYTES, and if all
// characters are hex values (00..ff).
bool valid_bignum(const char *value);

int bignum_cmp(const bignum *a, const bignum *b);

#endif
