/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

#include <meta_common.h>

int meta_verbose_level = 0;
int meta_indent_level = 0;

static void indent(int levels)
{
    while (levels-- > 0)
        putchar('\t');
}

void verbose(int level, const char *fmt, ...)
{
    if (level <= meta_verbose_level) {
        indent(meta_indent_level);
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
}

void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    meta_vsyslog(LOG_ERR, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

void die_perror(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "%s", strerror(errno));
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}

void warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    meta_vsyslog(LOG_WARNING, fmt, ap);
    va_end(ap);
}

/*
 * Aix has no vsyslog, so we use our own.
 * We limit the output to 1000 characters. That should be
 * sufficient for most error messages.
 */
void meta_vsyslog(int class, const char *fmt, va_list ap)
{
    char err[1000];
    vsnprintf(err, sizeof err, fmt, ap);

    // We need a destination regime as we sometimes need to see messages on console.
    if (1) {
        fprintf(stderr, "%s\n", err);
    }
    else {
        syslog(class, "%s", err);
    }
}

static int meta_debug_enabled;

void meta_enable_debug_output(void)
{
    meta_debug_enabled = 1;
}

void meta_disable_debug_output(void)
{
    meta_debug_enabled = 0;
}

void debugimpl(const char *fmt, ...)
{

    if (meta_debug_enabled) {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
}

status_t fail(int cause)
{
    assert(cause != 0);

    errno = cause;
    return failure;
}

void *xmalloc(size_t size)
{
    void *result;

    assert(size > 0);

    result = malloc(size);
    if (result == NULL)
        die("Out of memory\n");

    return result;
}

void *xcalloc(size_t nmemb, size_t size)
{
    void *result;

    assert(nmemb > 0);
    assert(size > 0);

    result = calloc(nmemb, size);
    if (result == NULL)
        die("Out of memory\n");

    return result;
}

void *xrealloc(void *mem, size_t size)
{
    void *result;

    assert(size > 0);

    result = realloc(mem, size);
    if (result == NULL)
        die("Out of memory\n");

    return result;
}

char *xstrdup(const char *src)
{
    char *result;

    assert(src != NULL);

    result = strdup(src);
    if (result == NULL)
        die("Out of memory\n");

    return result;
}
