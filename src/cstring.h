/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#ifdef __cplusplus
extern "C" {
#endif


typedef struct cstring_tag* cstring;
/*
 * Implementation of the cstring ADT.
 * We store the number of bytes allocated so that we know when we have to
 * reallocate. We also store the number of bytes used to avoid calling
 * strlen() all the time.
 */
struct cstring_tag
{
    size_t cbAllocated;
    size_t cbUsed;
    char* data;
};


cstring cstring_new(void);
cstring cstring_dup(const char* src); /* NEW: Create cstring and copy src in one operation */
int     cstring_multinew(cstring *pstr, size_t nelem);
int     cstring_extend(cstring s, size_t size);


int cstring_copy(cstring dest, const char* src);
int cstring_ncopy(cstring dest, const char* src, size_t n);
int cstring_charcat(cstring dest, int c);

int cstring_concat(cstring dest, const char* src);
int cstring_concat2(cstring dest, const char* src1, const char* src2);
int cstring_concat3(cstring dest, const char* src1, const char* src2, const char* src3);

int cstring_pcat(cstring dest, const char *start, const char *end);

int cstring_printf(cstring dest, size_t needs_max, const char* fmt, ...);
int cstring_vprintf(cstring dest, size_t needs_max, const char* fmt, va_list ap);

cstring cstring_left(cstring src, size_t n);
cstring cstring_right(cstring src, size_t n);
cstring cstring_substring(cstring src, size_t from, size_t to);

void cstring_reverse(cstring s);

static inline const char* c_str(cstring s)
{
    assert(NULL != s);
    assert(NULL != s->data);

    return s->data;
}

static inline size_t cstring_length(cstring s)
{
    assert(NULL != s);
    assert(NULL != s->data);
    assert((strlen(s->data) + 1) == s->cbUsed);

    return s->cbUsed - 1;
}

static inline void cstring_free(cstring s)
{
    if (s != NULL) {
        free(s->data);
        free(s);
    }
}

static inline int cstring_compare(cstring dest, const char* src)
{
    assert(NULL != dest);
    assert(NULL != src);
    assert(NULL != dest->data);

    return strcmp(dest->data, src);
}

static inline void cstring_recycle(cstring s)
{
    assert(NULL != s);
    assert(NULL != s->data);

    *s->data = '\0';
    s->cbUsed = 1;
}

/* NEW STUFF: 2005-12-06*/
/* Create an array of cstrings from a const char*, return the number
 * of items in the array. Each item must be freed separately.
 * delim can contain any number of characters, see strcspn()
 */
size_t cstring_split(cstring* dest, const char* src, const char* delim);

/* Free multiple cstrings with one call. Note that pstr itself will
 * not be freed. */
void cstring_multifree(cstring *pstr, size_t nelem);

/* Strip leading and trailing white space, ie whatever isspace() returns. */
void cstring_strip(cstring s);

/* Convert to lower/upper letters only */
void cstring_lower(cstring s);
void cstring_upper(cstring s);

#ifdef __cplusplus
}
#endif

#endif  /* guard */
