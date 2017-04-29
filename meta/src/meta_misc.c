/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

/*
 * File contains misc functions, some to replace non-ansi functions
 * like strcasecmp()
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <meta_misc.h>

status_t string2size_t(const char *s, size_t *val)
{
    assert(s != NULL);
    assert(val != NULL);

    *val = 0;
    while (*s) {
        if (!isdigit((int)*s))
            return failure;

        *val = (*val * 10) + (*s - '0');
        s++;
    }

    return success;
}

int find_word(const char *s, size_t iWord)
{
    const char *string = s;

    /* Loop 0..n times to skip words */
    assert(s != NULL);
    while (iWord--) {
        /* Skip until one space */
        while (*string != '\0' && *string != ' ')
            string++;

        /* Skip the space(even multiple) */
        while (*string != '\0' && *string == ' ')
            string++;
    }

    /* if index out of range */
    if (*string == '\0')
        return -1;

    return string - s;
}

int get_word_count(const char *s)
{
    int n = 0;
    int last_was_space = 0;

    assert(s != NULL);

    /* Remove leading ws */
    while (*s == ' ')
        s++;

    if (*s != '\0')
        n++;

    while (*s != '\0') {
        if (*s == ' ') {
            if (!last_was_space)
                n++;

            last_was_space = 1;
        }
        else
            last_was_space = 0;

        s++;
    }

    return n;
}

status_t get_word_from_string(
    const char *src,
    char dest[],
    size_t destsize,
    size_t iWord)		/* zero-based index of dest to copy */
{
    int i;

    assert(src != NULL);
    assert(dest != NULL);
    assert(destsize > 1);

    i = find_word(src, iWord);

    /* if index out of range */
    if (i == -1) {
        errno = ERANGE;
        return failure;
    }

    /* copy the word */
    src += i;
    return copy_word(src, dest, ' ', destsize);
}

void trim(char *s)
{
    ltrim(s);
    rtrim(s);
}

void ltrim(char *s)
{
    char *org = s;

    while (isspace(*s))
        s++;

    if (org != s)
        memmove(org, s, strlen(s) + 1);
}

void rtrim(char *s)
{
    size_t n = strlen(s);

    while (n-- > 0 && isspace(s[n]))
        s[n] = '\0';
}
        


status_t copy_word(
    const char *src,
    char dest[],
    int separator,
    size_t destsize)
{
    size_t i = 0;

    assert(src != NULL);
    assert(dest != NULL);
    assert(separator != '\0');
    assert(destsize > 0);

    while (*src != '\0' && *src != separator && i < destsize)
        dest[i++] = *src++;

    if (i == destsize) {
        errno = ENOSPC;
        return failure;
    }

    dest[i] = '\0';
    return success;
}

void remove_trailing_newline(char *s)
{
    size_t i;

    assert(s != NULL);

    i = strlen(s);
    if (i > 0) {
        i--;
        if (s[i] == '\n')
            s[i] = '\0';
    }
}

status_t get_extension(const char *src, char *dest, size_t destsize)
{
    const char *end;
    size_t i = destsize - 1;
    int found = 0;

    assert(src != NULL);
    assert(dest != NULL);
    assert(destsize > 1);

    end = src + strlen(src);
    dest[i] = '\0';

    while (end >= src && i > 0) {
        if (*end == '.') {
            found = 1;
            break;
        }

        dest[i--] = *end--;
    }

    if (i == 0) {
        errno = ENOSPC;
        return failure;
    }

    if (found)
        memmove(dest, &dest[i + 1], destsize - i);
    else
        *dest = '\0';

    return success;
}


/*
 * This is probably bad, but the need for speed forces us to
 * do it this way. I cannot see any security issues unless the
 * user of Highlander actually wants it to fail, so here we go.
 * Decide the mime type for a file, based on extension.
 */

const char *get_mime_type(const char *filename)
{
    static struct {
        const char *ext;
        const char *mime;
    } map[] = {
        { "css",	"text/css" },
        { "html",	"text/html" },
        { "htm",	"text/html" },
        { "c",		"text/plain" },
        { "cpp",	"text/plain" },
        { "cxx",	"text/plain" },
        { "h",		"text/plain" },
        { "java",	"text/plain" },
        { "txt",	"text/plain" },
        { "xml",	"text/xml" },
        { "rtf",	"text/rtf" },
        { "sgml",	"text/sgml" },

        { "jpeg",	"image/jpeg" },
        { "jpg",	"image/jpeg" },
        { "png",	"image/png" },
        { "tiff",	"image/tiff" },
        { "gif",	"image/gif" },
    };

    size_t i, nelem = sizeof(map) / sizeof(map[0]);
    char ext[100];

    if (get_extension(filename, ext, sizeof ext)) {
        for (i = 0; i < nelem; i++) {
            if (strcmp(map[i].ext, ext) == 0) {
                return map[i].mime;
            }
        }
    }

    return "application/octet-stream";
}

void fs_lower(char *s)
{
    assert(NULL != s);

    while (*s != '\0') {
        if (isupper((int)*s))
            *s = tolower((int)*s);

        s++;
    }
}


#ifdef CHECK_MISCFUNC

static void check_trim(void)
{
    size_t i, n;
    char buf[1024];

    static const struct {
        const char *in, *out;
    } tests[] = {
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
    };

    n = sizeof tests / sizeof *tests;
    for (i = 0; i < n; i++) {
        strcpy(buf, tests[i].in);
        trim(buf);
        if (strcmp(buf, tests[i].out))
            printf("Expected %s, got %s\n", tests[i].out, buf);
    }
}

static void check_ltrim(void)
{
    size_t i, n;
    char buf[1024];

    static const struct {
        const char *in, *out;
    } tests[] = {
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
        { "foo", "foo" },
    };

    n = sizeof tests / sizeof *tests;
    for (i = 0; i < n; i++) {
        strcpy(buf, tests[i].in);
        ltrim(buf);
        if (strcmp(buf, tests[i].out))
            printf("Expected %s, got %s\n", tests[i].out, buf);
    }
}

static void check_rtrim(void)
{
    size_t i, n;
    char buf[1024];

    static const struct {
        const char *in, *out;
    } tests[] = {
        { "", "" },
        { "	", "" },
        { "foo", "foo" },
        { "foo ", "foo" },
        { "foo 	 ", "foo" },
        { " foo", " foo" },
        { "foo", "foo" },
        { "foo", "foo" },
    };

    n = sizeof tests / sizeof *tests;
    for (i = 0; i < n; i++) {
        strcpy(buf, tests[i].in);
        rtrim(buf);
        if (strcmp(buf, tests[i].out))
            printf("Expected %s, got %s\n", tests[i].out, buf);
    }
}


int main(void)
{
    check_trim();
    check_ltrim();
    check_rtrim();
    return 0;
}
#endif
