/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>

#include <meta_common.h>
#include <cstring.h>

#define CSTRING_INITIAL_SIZE 256

/* Return 1 if the string has room for n new characters,
 * in addition to whatever number of characters it already
 * has.
 */
static inline int has_room_for(cstring s, size_t n)
{
    size_t freespace = s->size - s->len - 1;
    return n <= freespace;
}

status_t cstring_extend(cstring s, size_t size)
{
    size_t bytes_needed, newsize;
    char *data;

    assert(s != NULL);
    assert(s->data != NULL);
    assert(s->size >= CSTRING_INITIAL_SIZE);
    assert(s->len < s->size);

    /* Check for available space and reallocate if necessary */
    bytes_needed = s->len + size + 1;
    if (bytes_needed > s->size) {
        /* Double the size of the buffer */
        newsize = s->size * 2;
        if (newsize < bytes_needed) {
            /* Doubling wasn't sufficient */
            newsize = s->size + size;
        }

        if ((data = realloc(s->data, newsize)) == NULL)
            return failure;

        s->data = data;
        s->size = newsize;
    }

    assert(s->len == strlen(s->data));
    return success;
}

status_t cstring_vprintf(cstring dest, const char *fmt, va_list ap)
{
    int i;
    size_t size;
    va_list ap2;

    va_copy(ap2, ap);
    size = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    // size now contains the bytes needed _excluding_ the null char.
    // We must add 1 to avoid truncation, since vsnprintf() will not
    // write more than size bytes including the null character.
    size++;

    assert(dest != NULL);
    assert(dest->data != NULL);
    assert(dest->len == strlen(dest->data));

    if (!has_room_for(dest, size) && !cstring_extend(dest, size))
        return failure;

    // We append the new data, therefore the &
    i = vsnprintf(&dest->data[dest->len], size, fmt, ap);

    // We do not know the length of the data after vsnprintf()
    // We therefore recompute the len member.
    dest->len += i;

    assert(dest->len == strlen(dest->data));
    return success;
}

status_t cstring_printf(cstring dest, const char *fmt, ...)
{
    status_t status;
    va_list ap;

    assert(dest != NULL);
    assert(fmt != NULL);

    va_start(ap, fmt);
    status = cstring_vprintf(dest, fmt, ap);
    va_end(ap);

    return status;
}

status_t cstring_pcat(cstring dest, const char *start, const char *end)
{
    ptrdiff_t cb;

    assert(dest != NULL);
    assert(start != NULL);
    assert(end != NULL);
    assert(start < end);

    cb = end - start;
    if (!has_room_for(dest, cb) && !cstring_extend(dest, cb))
        return failure;

    memcpy(&dest->data[dest->len], start, cb);
    dest->len += cb;
    dest->data[dest->len] = '\0';

    assert(dest->len == strlen(dest->data));
    return success;
}

status_t cstring_concat(cstring dest, const char *src)
{
    size_t cb;

    assert(src != NULL);
    assert(dest != NULL);

    cb = strlen(src);
    if (!has_room_for(dest, cb) && !cstring_extend(dest, cb))
        return failure;

    /* Now add the string to the dest */
    strcat(&dest->data[dest->len], src);
    dest->len += cb;

    assert(dest->len == strlen(dest->data));
    return success;
}

status_t cstring_charcat(cstring dest, int c)
{
    assert(dest != NULL);

    if (!has_room_for(dest, 1) && !cstring_extend(dest, 1))
        return failure;

    dest->data[dest->len++] = c;
    dest->data[dest->len] = '\0';

    assert(dest->len == strlen(dest->data));
    return success;
}

cstring cstring_new(void)
{
    cstring p;

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    if ((p->data = malloc(CSTRING_INITIAL_SIZE)) == NULL) {
        free(p);
        return NULL;
    }

    p->len = 0;
    p->size = CSTRING_INITIAL_SIZE;
    *p->data = '\0';

    return p;
}

cstring cstring_dup(const char *src)
{
    cstring dest;

    assert(src != NULL);

    if ((dest = cstring_new()) == NULL)
        return NULL;

    if (!cstring_set(dest, src)) {
        cstring_free(dest);
        return NULL;
    }

    return dest;
}

status_t cstring_copy(cstring dest, const cstring src)
{
    assert(dest != NULL);
    assert(src != NULL);

    return cstring_set(dest, src->data);
}

status_t cstring_set(cstring dest, const char *src)
{
    size_t n;

    assert(dest != NULL);
    assert(dest->data != NULL);
    assert(src != NULL);

    cstring_recycle(dest);
    n = strlen(src);
    if (!has_room_for(dest, n) && !cstring_extend(dest, n))
        return failure;

    strcpy(dest->data, src);
    dest->len += n;

    assert(dest->len == strlen(dest->data));
    return success;
}

status_t cstring_nset(cstring dest, const char *src, const size_t cch)
{
    size_t len;

    assert(dest != NULL);
    assert(dest->data != NULL);
    assert(src != NULL);

    cstring_recycle(dest);
    len = strlen(src);
    if (len > cch)
        len = cch;

    if (!has_room_for(dest, len) && !cstring_extend(dest, len))
        return failure;

    strncpy(dest->data, src, len);
    dest->data[len] = '\0';
    dest->len = len;

    assert(dest->len == strlen(dest->data));
    return success;
}

status_t cstring_concat2(cstring dest, const char *s1, const char *s2)
{
    assert(dest != NULL);
    assert(s1 != NULL);
    assert(s2 != NULL);

    if (cstring_concat(dest, s1) && cstring_concat(dest, s2))
        return success;

    return failure;
}

status_t cstring_concat3(cstring dest, const char *s1, const char *s2,
    const char *s3)
{
    assert(dest != NULL);
    assert(s1 != NULL);
    assert(s2 != NULL);
    assert(s3 != NULL);

    if (cstring_concat(dest, s1)
    && cstring_concat(dest, s2)
    && cstring_concat(dest, s3))
        return success;

    return failure;
}

status_t cstring_multinew(cstring* pstr, size_t nelem)
{
    size_t i;

    assert(pstr != NULL);
    assert(nelem > 0);

    for (i = 0; i < nelem; i++) {
        if ((pstr[i] = cstring_new()) == NULL) {
            while (i-- > 0)
                cstring_free(pstr[i]);

            return failure;
        }
    }

    return success;
}

/* Free multiple cstrings with one call */
void cstring_multifree(cstring *pstr, size_t nelem)
{
    size_t i;

    assert(pstr != NULL);

    for (i = 0; i < nelem; i++)
        cstring_free(pstr[i]);
}

cstring cstring_left(cstring src, size_t n)
{
    cstring dest;

    assert(src != NULL);

    if ((dest = cstring_new()) == NULL)
        return NULL;

    if (!cstring_extend(dest, n)) {
        cstring_free(dest);
        return NULL;
    }

    if (!cstring_nset(dest, src->data, n)) {
        cstring_free(dest);
        return NULL;
    }

    return dest;
}

cstring cstring_right(cstring src, size_t n)
{
    cstring dest;
    const char *s;
    size_t cb;

    /* Get mem */
    if ((dest = cstring_new()) == NULL)
        return NULL;

    if (!cstring_extend(dest, n)) {
        cstring_free(dest);
        return NULL;
    }

    s = src->data;
    cb = strlen(s);

    /* Copy string */
    if (cb > n)
        s += cb - n;

    if (!cstring_set(dest, s)) {
        cstring_free(dest);
        return NULL;
    }

    return dest;
}

cstring cstring_substring(cstring src, size_t from, size_t to)
{
    cstring dest;
    size_t cb;

    assert(src != NULL);
    assert(from <= to);
    assert(to <= src->len);

    if (to > src->len)
        to = src->len;

    cb = to - from + 1;
    if ((dest = cstring_new()) == NULL)
        return NULL;

    if (!cstring_extend(dest, cb)) {
        cstring_free(dest);
        return NULL;
    }

    if (!cstring_pcat(dest, &src->data[from], &src->data[to])) {
        cstring_free(dest);
        return NULL;
    }

    return dest;
}

void cstring_reverse(cstring s)
{
    if (s->len > 0) {
        char *beg = s->data;
        char *end = s->data + s->len - 1;

        while (beg < end) {
            char tmp = *end;
            *end-- = *beg;
            *beg++ = tmp;
        }
    }
}

/*
 * 1. Count the number of words
 * 2. Allocate space for an array of cstrings.
 * 3. Allocate each cstring.
 * 4. Copy each word
 */
size_t cstring_split(cstring** dest, const char *src, const char *delim)
{
    const char *s;
    size_t end, i, len, nelem;

    assert(dest != NULL);
    assert(src != NULL);
    assert(delim != NULL);

    /* Skip to first substring */
    len = strlen(src);
    s = src + strspn(src, delim);

    /* Only delimiters in src */
    if (s - src == (int)len)
        return 0;

    /* Count elements */
    for (nelem = 0; *s != '\0'; nelem++) {
        s += strcspn(s, delim);
        s += strspn(s, delim);
    }

    /* allocate space */
    if ((*dest = malloc(sizeof *dest * nelem)) == NULL)
        return 0;

    if (cstring_multinew(*dest, nelem) == 0) {
        free(*dest);
        return 0;
    }

    /* Now copy */
    s = src + strspn(src, delim); /* start of first substring */
    for (i = 0; *s != '\0'; i++) {
        end = strcspn(s, delim);
        if (!cstring_pcat((*dest)[i], s, s + end)) {
            cstring_multifree(*dest, nelem);
            return 0;
        }

        s += end;
        s += strspn(s, delim);
    }

    return nelem;
}

void cstring_strip(cstring s)
{
    size_t i;

    assert(s != NULL);

    /* strip trailing ws first */
    i = s->len;
    while (i-- > 0 && isspace((unsigned char)s->data[i])) {
        s->data[i] = '\0';
        s->len--;
    }

    /* Now leading ws */
    for (i = 0; i < s->len; i++) {
        if (!isspace((unsigned char)s->data[i]))
            break;
    }

    if (i > 0) {
        s->len -= i;
        memmove(&s->data[0], &s->data[i], s->len);
        s->data[s->len] = '\0';
    }
}

void cstring_lower(cstring s)
{
    size_t i;

    assert(s != NULL);

    for (i = 0; i < s->len; i++) {
        if (isupper((unsigned char)s->data[i]))
            s->data[i] = tolower((unsigned char)s->data[i]);
    }
}

void cstring_upper(cstring s)
{
    size_t i;

    assert(s != NULL);

    for (i = 0; i < s->len; i++) {
        if (islower((unsigned char)s->data[i]))
            s->data[i] = toupper((unsigned char)s->data[i]);
    }
}


#ifdef CHECK_CSTRING
int main(void)
{
    cstring s, dest, *pstr;
    int rc;
    const char *start = "This is a string";
    const char *end = start + strlen(start);
    status_t status;

    char longstring[10000];

    size_t i, nelem = 100;

    memset(longstring, 'A', sizeof longstring);
    longstring[sizeof longstring - 1] = '\0';

    for (i = 0; i < nelem; i++) {
        s = cstring_new();
        assert(s != NULL);

        status = cstring_set(s, "Hello");
        assert(status == success);

        rc = cstring_compare(s, "Hello");
        assert(rc == 0);

        rc = cstring_compare(s, "hello");
        assert(rc != 0);

        status = cstring_concat(s, ", world");
        assert(status == success);

        rc = cstring_compare(s, "Hello, world");
        assert(rc == 0);

        status = cstring_concat(s, longstring);
        assert(status == success);

        status = cstring_charcat(s, 'A');
        assert(status == success);

        cstring_recycle(s);
        status = cstring_concat(s, longstring);
        assert(status == success);

        status = cstring_concat2(s, longstring, longstring);
        assert(status == success);

        status = cstring_concat3(s, longstring, longstring, longstring);
        assert(status == success);

        /* Test strpcat */
        cstring_recycle(s);
        status = cstring_pcat(s, start, end);
        assert(status == success);

        rc = cstring_compare(s, start);
        assert(rc == 0);

        /* Test cstring_left() */
        status = cstring_set(s, "hello, world");
        dest = cstring_left(s, 5);
        rc = cstring_compare(dest, "hello");
        assert(rc == 0);
        cstring_free(dest);

        /* Test cstring_left() with short strings */
        dest = cstring_left(s, 5000);
        rc = cstring_compare(dest, "hello, world");
        assert(rc == 0);
        cstring_free(dest);

        /* cstring_right() */
        status = cstring_set(s, "hello, world");
        dest = cstring_right(s, 5);
        rc = cstring_compare(dest, "world");
        assert(rc == 0);
        cstring_free(dest);

        dest = cstring_right(s, 5000);
        rc = cstring_compare(dest, "hello, world");
        assert(rc == 0);
        cstring_free(dest);

        /* cstring_substring */
        status = cstring_set(s, "hello, world");
        dest = cstring_substring(s, 0, 5);
        rc = cstring_compare(dest, "hello");
        assert(rc == 0);
        cstring_free(dest);

        dest = cstring_substring(s, 1, 5);
        rc = cstring_compare(dest, "ello");
        assert(rc == 0);
        cstring_free(dest);

        dest = cstring_substring(s, 7, 12);
        rc = cstring_compare(dest, "world");
        assert(rc == 0);
        cstring_free(dest);

        /* cstring_reverse */
        status = cstring_set(s, "hello, world");
        cstring_reverse(s);
        rc = cstring_compare(s, "dlrow ,olleh");
        assert(rc == 0);
        /* cstring_strip */

        status = cstring_set(s, "  a b c d e f	");
        cstring_strip(s);
        rc = cstring_compare(s, "a b c d e f");
        assert(rc == 0);

        cstring_upper(s);
        rc = cstring_compare(s, "A B C D E F");
        assert(rc == 0);

        cstring_lower(s);
        rc = cstring_compare(s, "a b c d e f");
        assert(rc == 0);

        cstring_free(s);

        /* cstring_split() */
        rc = cstring_split(&pstr, "foo bar baz", " ");
        assert(rc == 3);
        cstring_multifree(pstr, rc);
        free(pstr);

        rc = cstring_split(&pstr, "          foo bar baz", " ");
        assert(rc == 3);
        cstring_multifree(pstr, rc);
        free(pstr);

        rc = cstring_split(&pstr, "       foo bar baz      ", " ");
        assert(rc == 3);
        cstring_multifree(pstr, rc);
        free(pstr);

        rc = cstring_split(&pstr, "       foo ", " ");
        assert(rc == 1);
        cstring_multifree(pstr, rc);
        free(pstr);

        // Test the printf functions
        s = cstring_new();
        assert(s != NULL);

        status = cstring_printf(s, "Hello");
        assert(status == success);
        rc = cstring_compare(s, "Hello");
        assert(rc == 0);

        cstring_recycle(s);
        status = cstring_printf(s, "%s %s", "Hello", "world");
        assert(status == success);
        rc = cstring_compare(s, "Hello world");
        assert(rc == 0);
        assert(cstring_length(s) == 11);

    }

    return 0;
}
#endif
