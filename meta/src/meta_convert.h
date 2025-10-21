#ifndef META_CONVERT_H
#define META_CONVERT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <meta_common.h>

/* Conversion of strings to numbers */

// Do we need a prefix, as in toint, touint, tofloat, todouble?
// Let's try with to*(), even if it's a little too general.

status_t toint(const char *s, int *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t touint(const char *s, unsigned *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t touint16_t(const char *s, uint16_t *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t tolong(const char *s, long *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t toulong(const char *s, unsigned long *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t tosize_t(const char *s, size_t *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t hextosize_t(const char *s, size_t *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t tofloat(const char *s, float *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

status_t todouble(const char *s, double *dest)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool isint(const char *s)
{
    int d;
    return toint(s, &d) != failure;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool isuint(const char *s)
{
    unsigned int d;
    return touint(s, &d) != failure;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool isuint16_t(const char *s)
{
    uint16_t d;
    return touint16_t(s, &d) != failure;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool issize_t(const char *s)
{
    size_t d;
    return tosize_t(s, &d) != failure;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool islong(const char *s)
{
    long d;
    return tolong(s, &d) != failure;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool isulong(const char *s)
{
    unsigned long d;
    return toulong(s, &d) != failure;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool isfloat(const char *s)
{
    float d;
    return tofloat(s, &d) != failure;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool isdouble(const char *s)
{
    double d;
    return todouble(s, &d) != failure;
}


#endif
