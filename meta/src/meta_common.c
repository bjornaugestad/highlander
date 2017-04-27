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

#include <assert.h>
#include <ctype.h>
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

void fs_lower(char *s)
{
    assert(NULL != s);

    while (*s != '\0') {
        if (isupper((int)*s))
            *s = tolower((int)*s);

        s++;
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
