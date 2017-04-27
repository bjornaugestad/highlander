/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef CSTRING_H
#define CSTRING_H

#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

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
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

status_t cstring_multinew(cstring *pstr, size_t nelem) 
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t cstring_extend(cstring s, size_t size) 
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t cstring_copy(cstring dest, const cstring src) 
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t cstring_set(cstring dest, const char *src) 
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t cstring_nset(cstring dest, const char *src, size_t n) 
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t cstring_charcat(cstring dest, int c) 
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t cstring_concat(cstring dest, const char *src) 
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t cstring_concat2(cstring dest, const char *src1, const char *src2) 
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

status_t cstring_concat3(cstring dest, const char *src1, const char *src2,
    const char *src3) 
    __attribute__((nonnull(1, 2, 3, 4)))
    __attribute__((warn_unused_result));

status_t cstring_pcat(cstring dest, const char *start, const char *end) 
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

status_t cstring_printf(cstring dest, size_t needs_max, const char *fmt, ...)
    __attribute__((nonnull(1, 3)))
    __attribute__((format(printf, 3, 4))) 
    __attribute__((warn_unused_result));

status_t cstring_vprintf(cstring dest, size_t needs_max,
    const char *fmt, va_list ap) 
    __attribute__((nonnull(1, 3)))
    __attribute__((warn_unused_result));

cstring cstring_left(cstring src, size_t n) 
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

cstring cstring_right(cstring src, size_t n) 
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

cstring cstring_substring(cstring src, size_t from, size_t to) 
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void cstring_reverse(cstring s) __attribute__((nonnull(1)));

static inline const char *c_str(cstring s)
{
    assert(NULL != s);
    assert(NULL != s->data);

    return s->data;
}

static inline size_t cstring_length(cstring s)
{
    assert(NULL != s);
    assert(NULL != s->data);
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

static inline int cstring_compare(cstring s, const char *str)
{
    assert(NULL != s);
    assert(NULL != str);
    assert(NULL != s->data);

    return strcmp(s->data, str);
}

static inline int cstring_casecompare(cstring s, const char *str)
{
    assert(NULL != s);
    assert(NULL != str);
    assert(NULL != s->data);

    return strcasecmp(s->data, str);
}


static inline void cstring_recycle(cstring s)
{
    assert(NULL != s);
    assert(NULL != s->data);

    *s->data = '\0';
    s->len = 0;
}

/* Create an array of cstrings from a const char*, return the number
 * of items in the array. Each item must be freed separately.
 * delim can contain any number of characters, see strcspn()
 */
size_t cstring_split(cstring** dest, const char *src, const char *delim);

/* Free multiple cstrings with one call. Note that pstr itself will
 * not be freed. */
void cstring_multifree(cstring *pstr, size_t nelem)
    __attribute__((nonnull(1)));

/* Strip leading and trailing white space, ie whatever isspace() returns. */
void cstring_strip(cstring s) __attribute__((nonnull(1)));

/* Convert to lower/upper letters only */
void cstring_lower(cstring s) __attribute__((nonnull(1)));
void cstring_upper(cstring s) __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
