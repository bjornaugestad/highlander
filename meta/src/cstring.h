/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef CSTRING_H
#define CSTRING_H

#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Implementation of the cstring ADT.
 * We store the number of bytes allocated so that we know when we have to
 * reallocate. We also store the number of bytes used to avoid calling
 * strlen() all the time.
 */
struct cstring_tag {
    size_t size; /* Size of allocated buffer */
    size_t len;  /* Length of string, excluding \0 */
    char *data;
};
typedef struct cstring_tag* cstring;

cstring cstring_new(void)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

cstring cstring_dup(const char *src)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

status_t cstring_multinew(cstring *pstr, size_t nelem)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_extend(cstring s, size_t size)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_set(cstring dest, const char *src)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_nset(cstring dest, const char *src, size_t n)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_charcat(cstring dest, int c)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_concat(cstring dest, const char *src)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_concat2(cstring dest, const char *src1, const char *src2)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_concat3(cstring dest, const char *src1, const char *src2,
    const char *src3)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_pcat(cstring dest, const char *start, const char *end)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t cstring_printf(cstring dest, const char *fmt, ...)
    __attribute__((nonnull))
    __attribute__((format(printf, 2, 3)))
    __attribute__((warn_unused_result));

status_t cstring_vprintf(cstring dest, const char *fmt, va_list ap)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

cstring cstring_left(cstring src, size_t n)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

cstring cstring_right(cstring src, size_t n)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

cstring cstring_substring(cstring src, size_t from, size_t to)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void cstring_reverse(cstring s) __attribute__((nonnull));

__attribute__((warn_unused_result))
__attribute__((nonnull))
static inline const char *c_str(cstring s)
{
    assert(s != NULL);
    assert(s->data != NULL);

    return s->data;
}

__attribute__((warn_unused_result))
__attribute__((nonnull))
static inline size_t cstring_length(cstring s)
{
    assert(s != NULL);
    assert(s->data != NULL);
    assert(strlen(s->data) == s->len);

    return s->len;
}

static inline void cstring_free(cstring s)
{
    if (s != NULL) {
        free(s->data);
        free(s);
    }
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline int cstring_compare(cstring s, const char *str)
{
    assert(s != NULL);
    assert(str != NULL);
    assert(s->data != NULL);

    return strcmp(s->data, str);
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline int cstring_casecompare(cstring s, const char *str)
{
    assert(s != NULL);
    assert(str != NULL);
    assert(s->data != NULL);

    return strcasecmp(s->data, str);
}


__attribute__((nonnull))
static inline void cstring_recycle(cstring s)
{
    assert(s != NULL);
    assert(s->data != NULL);

    *s->data = '\0';
    s->len = 0;
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline status_t
cstring_copy(cstring dest, const cstring src)
{
    assert(dest != NULL);
    assert(src != NULL);

    return cstring_set(dest, src->data);
}

__attribute__((nonnull))
__attribute__((warn_unused_result))
static inline bool
cstring_equal(cstring lhs, const cstring rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    return lhs->len == rhs->len && strcmp(lhs->data, rhs->data) == 0;
}


/* Create an array of cstrings from a const char*, return the number
 * of items in the array. Each item must be freed separately.
 * delim can contain any number of characters, see strcspn()
 */
size_t cstring_split(cstring** dest, const char *src, const char *delim)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

/* Free multiple cstrings with one call. Note that pstr itself will
 * not be freed. */
void cstring_multifree(cstring *pstr, size_t nelem)
    __attribute__((nonnull));

/* Strip leading and trailing white space, ie whatever isspace() returns. */
void cstring_strip(cstring s) __attribute__((nonnull));

// Strip right (trailing) ws */
void cstring_rstrip(cstring s) __attribute__((nonnull));

/* Convert to lower/upper letters only */
void cstring_lower(cstring s) __attribute__((nonnull));
void cstring_upper(cstring s) __attribute__((nonnull));

/* New stuff 2018: We need to be able do insert, replace and remove stuff.
 * Interface may look odd with all the offsets and n values, but the
 * intention is to combine cstring with meta_regex to do regular expression
 * search and replace. */

// Insert src in dest at offset. 
status_t cstring_insert(cstring dest, size_t offset, const char *src)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

// Remove n characters at offset.
void cstring_cut(cstring s, size_t offset, size_t n)
    __attribute__((nonnull));

// Remove all charactes to the right of offset. IOW, just write a
// \0 at offset.
void cstring_truncate(cstring s, size_t offset)
    __attribute__((nonnull));

// Replace 1..n bytes at offset with the string 'to'.
status_t cstring_replace(cstring s, size_t offset, size_t n, const char *to)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

int cstring_find(cstring s, int c)
    __attribute__((nonnull));

int cstring_findstr(cstring s, const char *str)
    __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
