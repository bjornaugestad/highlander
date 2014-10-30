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


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SENDFILE
#include <sys/sendfile.h>
#endif


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <meta_list.h>
#include <meta_misc.h>

#include "internals.h"
/*
 * NOTE: Since we are an originating server, there is no need
 * to send Age. Only caches send this one.
 */

/*
 * Responses have only 28 flags, so we go for only one
 * group of flags. use response_set_flag() and response_is_flag_set()
 * anyway, in case the number of flags grow.
 */
#define ACCEPT_RANGES		(0x1)
#define AGE					(0x2)
#define ALLOW				(0x4)
#define CACHE_CONTROL		(0x8)
#define CONTENT_ENCODING	(0x20)
#define CONTENT_LANGUAGE	(0x40)
#define CONTENT_LENGTH		(0x80)
#define CONTENT_LOCATION	(0x100)
#define CONTENT_MD5			(0x200)
#define CONTENT_RANGE		(0x400)
#define CONTENT_TYPE		(0x800)
#define DATE				(0x1000)
#define ETAG				(0x2000)
#define EXPIRES				(0x4000)
#define LAST_MODIFIED		(0x8000)
#define LOCATION			(0x10000)
#define PROXY_AUTHENTICATE	(0x40000)
#define RETRY_AFTER			(0x80000)
#define SERVER				(0x100000)
#define TRAILER				(0x200000)
#define UPGRADE				(0x800000)
#define VARY				(0x1000000)
#define WWW_AUTHENTICATE	(0x2000000)

/* Local helper functions */
static int response_send_header_fields(http_response p, connection conn);
static int response_send_cookies(http_response p, connection conn, meta_error e);
static int response_flag_isset(http_response p, unsigned long flag);
static void response_set_flag(http_response p, unsigned long flag);
static void response_clear_flags(http_response p);
static int strcat_quoted(cstring dest, const char* s);
static int send_entire_file(connection conn, const char *path, size_t *pcb);

/* Here is the response we are creating */
struct http_response_tag {
    http_version version;
    int status; /* The http status code we send back */

    general_header general_header;
    entity_header entity_header;

    /* Contains one bit for each field. "true" if field contains value. */
    unsigned long flags;

    /*
     * 4 of these fields are common to http 1.0 and http1.1
     * The fields are:
     *	location		See rfc1945, §10.11 for 1.0 doc
     *	server			See rfc1945, §10.14 for 1.0 doc
     *	www_authenticate	See rfc1945, §10.16 for 1.0 doc
     *	retry_after		See rfc1945, §D.2.8 for 1.0 doc
     *
     * All other fields are http 1.1 specific, but some are
     * commonly used as an extension of http 1.0, e.g. Host
     */

    unsigned long age;
    int accept_ranges;			/* §14.5 */
    cstring etag;				/* §14.19 */
    cstring location;			/* §14.30 */
    cstring proxy_authenticate;	/* §14.33 */
    time_t retry_after;			/* §14.38 */
    cstring server;				/* §14.39 */
    cstring vary;				/* §14.44 */
    cstring www_authenticate;	/* §14.47 */

    /* Outgoing cookies */
    list cookies;

    /* We unfortunately need to store everything here to
     * support cookies properly */
    cstring entity;

    /* The page function can assign its own content buffer.  */
    void* content_buffer;
    int content_buffer_in_use;
    int content_free_when_done;

    /* sometimes we want to send an entire file instead of
     * regular content. */
    int send_file;
    cstring path;
};

general_header response_get_general_header(http_response r)
{
    return r->general_header;
}

entity_header response_get_entity_header(http_response r)
{
    return r->entity_header;
}

const char* response_get_entity(http_response p)
{
    assert(NULL != p);

    if (p->content_buffer_in_use)
        return p->content_buffer;
    else
        return c_str(p->entity);
}

void response_set_version(http_response r, http_version v)
{
    r->version = v;
}

http_response response_new(void)
{
    http_response p;
    cstring arr[8];
    size_t nelem = sizeof arr / sizeof *arr;

    if ((p = calloc(1, sizeof *p)) == NULL
    || !cstring_multinew(arr, nelem)) {
        free(p);
        return NULL;
    }

    if ((p->general_header = general_header_new()) == NULL
    ||  (p->entity_header = entity_header_new()) == NULL) {
        general_header_free(p->general_header);
        cstring_multifree(arr, nelem);
        free(p);
        return NULL;
    }

    if ((p->cookies = list_new()) == NULL) {
        general_header_free(p->general_header);
        entity_header_free(p->entity_header);
        cstring_multifree(arr, nelem);
        free(p);
        return NULL;
    }


    p->version = VERSION_UNKNOWN;
    p->status = 0;
    p->flags = 0;
    p->retry_after = (time_t)-1;
    p->send_file = 0;

    p->etag = arr[0];
    p->location = arr[1];
    p->proxy_authenticate = arr[2];
    p->server = arr[3];
    p->vary = arr[4];
    p->www_authenticate = arr[5];
    p->entity = arr[6];
    p->path = arr[7];

    /* Some defaults */
    if (!response_set_content_type(p, "text/html")
    || !response_set_server(p, "Highlander")) {
        response_free(p);
        p = NULL;
    }

    return p;
}

void response_set_status(http_response r, int status)
{
    assert(r != NULL);

    r->status = status;
}

void response_set_age(http_response r, unsigned long age)
{
    assert(r != NULL);
    r->age = age;
    response_set_flag(r, AGE);
}

int response_get_status(http_response r)
{
    assert(r != NULL);

    return r->status;
}

static int response_send_header(
    http_response response,
    connection conn,
    meta_error e)
{
    if (response->version == VERSION_09)
        /* No headers for http 0.9 */
        return 0;

    /* Special stuff to support pers. conns in HTTP 1.0 */
    if (connection_is_persistent(conn) && response->version == VERSION_10) {
        if (!response_set_connection(response, "Keep-Alive")) {
            return set_os_error(e, errno);
        }
    }

    if (!response_send_header_fields(response, conn))
        return set_tcpip_error(e, errno);

    /* Send cookies, if any */
    if (response->cookies != NULL) {
        if (!response_send_cookies(response, conn, e))
            return 0;
    }

    /* Send the \r\n separating all headers from an optional entity */
    if (!connection_write(conn, "\r\n", 2))
        return set_tcpip_error(e, errno);

    return 1;
}

int response_add(const http_response p, const char* value)
{
    assert(NULL != p);
    assert(NULL != value);

    return cstring_concat(p->entity, value);
}

int response_add_char(http_response p, int c)
{
    assert(NULL != p);
    return cstring_charcat(p->entity, c);
}

int response_add_end(http_response response, const char* start, const char* end)
{
    return cstring_pcat(response->entity, start, end);
}

void response_free(http_response p)
{
    if (p != NULL) {
        /* Free cookies */
        list_free(p->cookies, (void(*)(void*))cookie_free);

        general_header_free(p->general_header);
        entity_header_free(p->entity_header);
        cstring_free(p->etag);
        cstring_free(p->location);
        cstring_free(p->proxy_authenticate);
        cstring_free(p->server);
        cstring_free(p->vary);
        cstring_free(p->www_authenticate);
        cstring_free(p->entity);
        cstring_free(p->path);

        free(p);
    }
}

int response_printf(http_response page, const size_t needs_max, const char* fmt, ...)
{
    int success;
    va_list ap;

    assert(NULL != page);
    assert(NULL != fmt);

    va_start (ap, fmt);
    success = cstring_vprintf(page->entity, needs_max, fmt, ap);
    va_end(ap);

    return success;
}

/* return 1 if string needs to be quoted, 0 if not */
static int need_quote(const char* s)
{
    while (*s != '\0') {
        if (!isalnum(*s) && *s != '_')
            return 1;

        s++;
    }

    return 0;
}


/*
 * Creates a string consisting of the misc elements in a cookie.
 * String must have been new'ed.
 * Returns 0 on errors.
 */
static int create_cookie_string(cookie c, cstring str)
{
    const char* s;
    int v;

    if ((s = cookie_get_name(c)) == NULL)
        return 0;
    else if(!cstring_copy(str, "Set-Cookie: "))
        return 0;
    else if(!cstring_concat(str, s))
        return 0;

    /* Now get value and append. Remember to quote value if needed
     * NOTE: Netscape chokes, acc. to rfc2109, on quotes.
     * We therefore need to know the version and at least "quote when needed"
     */
    s = cookie_get_value(c);
    if (s) {
        if (!cstring_charcat(str, '='))
            return 0;
        else if(need_quote(c_str(str)) && !strcat_quoted(str, s))
            return 0;
        else if(!cstring_concat(str, s))
            return 0;
    }

    v = cookie_get_version(c);
    if (!cstring_printf(str, 20, ";Version=%d", v))
        return 0;

    v = cookie_get_max_age(c);
    if (v != MAX_AGE_NOT_SET
    && !cstring_printf(str, 20, ";Max-Age=%d", v))
        return 0;

    v = cookie_get_secure(c);
    if (!cstring_printf(str, 20, ";Secure=%d", v))
        return 0;

    s = cookie_get_domain(c);
    if (s != NULL && !cstring_concat2(str, ";Domain=", s))
        return 0;

    s = cookie_get_comment(c);
    if (s != NULL && !cstring_concat2(str, ";Comment=", s))
        return 0;

    s = cookie_get_path(c);
    if (s != NULL && !cstring_concat2(str, ";Path=", s))
        return 0;

    return cstring_concat(str, "\r\n");
}

/* return 0 on error, 1 on success. */
static int send_cookie(cookie c, connection conn, meta_error e)
{
    const char* s;
    cstring str;

    assert(NULL != c);
    assert(NULL != conn);

    /* A cookie with no name ? */
    if ((s = cookie_get_name(c)) == NULL) {
        return set_app_error(e, EFS_INTERNAL);
    }
    else if((str = cstring_new()) == NULL) {
        return set_os_error(e, ENOMEM);
    }
    else if(!create_cookie_string(c, str)) {
        cstring_free(str);
        return set_os_error(e, ENOMEM);
    }
    else {
        size_t cb = cstring_length(str);
        if (!connection_write(conn, c_str(str), cb)) {
            set_tcpip_error(e, errno);
            cstring_free(str);
            return 0;
        }
    }

    cstring_free(str);
    return 1;
}

static int response_send_cookies(http_response p, connection conn, meta_error e)
{
    list_iterator i;

    assert(NULL != p);
    assert(NULL != conn);

    for (i = list_first(p->cookies); !list_end(i); i = list_next(i)) {
        cookie c;

        c = list_get(i);
        if (!send_cookie(c, conn, e))
            return 0;
    }

    return 1;
}

int response_set_cookie(http_response response, cookie new_cookie)
{
    list_iterator i;
    const char* nameNew;

    nameNew = cookie_get_name(new_cookie);

    /* Lookup cookie to check for duplicates */
    for (i = list_first(response->cookies); !list_end(i); i = list_next(i)) {
        const char* nameOld;
        cookie c;

        c = list_get(i);
        assert(NULL != c);
        nameOld = cookie_get_name(c);
        if (0 == strcmp(nameNew, nameOld)) {
            /* We have a duplicate */
            errno = EINVAL;
            return 0;
        }
    }

    if (!list_add(response->cookies, new_cookie))
        return 0;
    else
        return 1;
}

int http_send_date(connection conn, const char* name, time_t value)
{
    char date[100];
    size_t cb;
    struct tm t, *ptm = &t;

    assert(name != NULL);

    cb = strlen(name);
    if (connection_write(conn, name, cb)) {
        gmtime_r(&value, ptm);
        cb = strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT\r\n", ptm);
        return connection_write(conn, date, cb);
    }
    else
        return 0;
}

int http_send_string(connection conn, const char* s)
{
    assert(s != NULL);

    size_t cb = strlen(s);
    return connection_write(conn, s, cb);
}

int http_send_ulong(connection conn, const char* name, unsigned long value)
{
    char val[1000];
    size_t cb = snprintf(val, sizeof val, "%s%lu", name, value);
    if (cb >= sizeof val)
        return 0;

    return connection_write(conn, val, cb);
}

int http_send_field(connection conn, const char* name, cstring value)
{
    size_t cb;

    assert(NULL != conn);
    assert(NULL != name);
    assert(NULL != value);

    cb = strlen(name);
    if (connection_write(conn, name, cb)) {
        cb = cstring_length(value);
        if (connection_write(conn, c_str(value), cb))
            return connection_write(conn, "\r\n", 2);
        else
            return 0;
    }
    else
        return 0;
}

size_t response_get_content_length(http_response p)
{
    size_t content_length = 0;

    assert(p != NULL);
    assert(p->entity != NULL);

    if (entity_header_content_length_isset(p->entity_header)) {
        content_length = entity_header_get_content_length(p->entity_header);
    }
    else {
        /* Shot in the dark, will not work for static pages */
        content_length = cstring_length(p->entity);
    }

    return content_length;
}


static inline int send_age(connection c, http_response p)
{
    return http_send_ulong(c, "Age: ", p->age);
}

static inline int send_etag(connection conn, http_response p)
{
    return http_send_field(conn, "ETag: ", p->etag);
}

static inline int send_location(connection conn, http_response p)
{
    return http_send_field(conn, "Location: ", p->location);
}

static inline int send_proxy_authenticate(connection conn, http_response p)
{
    return http_send_field(conn, "Proxy-Authenticate: ", p->proxy_authenticate);
}

static inline int send_server(connection conn, http_response p)
{
    return http_send_field(conn, "Server: ", p->server);
}

static inline int send_vary(connection conn, http_response p)
{
    return http_send_field(conn, "Vary: ", p->vary);
}

static inline int send_www_authenticate(connection conn, http_response p)
{
    return http_send_field(conn, "WWW-Authenticate: ", p->www_authenticate);
}


static inline int send_retry_after(connection conn, http_response p)
{
    return http_send_date(conn, "Retry-After: ", p->retry_after);
}

static inline int send_accept_ranges(connection conn, http_response p)
{
    size_t cch;
    const char* s;

    if (p->accept_ranges == 1)
        s = "Accept-Ranges: bytes\r\n";
    else
        s = "Accept-Ranges: none\r\n";

    cch = strlen(s);
    return connection_write(conn, s, cch);
}

static int response_send_header_fields(http_response p, connection conn)
{
    int success = 1;
    size_t i, cFields;

    static const struct {
        size_t flag;
        int (*func)(connection, http_response);
    } fields[] = {
        { AGE,					send_age },
        { ETAG,					send_etag },
        { LOCATION,				send_location },
        { PROXY_AUTHENTICATE,	send_proxy_authenticate },
        { SERVER,				send_server },
        { VARY,					send_vary },
        { WWW_AUTHENTICATE,		send_www_authenticate },
        { ACCEPT_RANGES,		send_accept_ranges },
        { RETRY_AFTER,			send_retry_after },
    };

    assert(NULL != p);
    assert(NULL != conn);

    /*
     * Some fields are required by http. We add them if the
     * user hasn't added them manually.
     */
    if (!general_header_date_isset(p->general_header))
        general_header_set_date(p->general_header, time(NULL));

    general_header_send_fields(p->general_header, conn);
    entity_header_send_fields(p->entity_header, conn);


    if (success) {
        cFields = sizeof(fields) / sizeof(fields[0]);
        for (i = 0; i < cFields; i++) {
            if (response_flag_isset(p, fields[i].flag)) {
                success = (*fields[i].func)(conn, p);
                if (!success)
                    break;
            }
        }
    }

    return success;
}

int response_set_connection(http_response response, const char* value)
{
    assert(NULL != response);

    return general_header_set_connection(response->general_header, value);
}

void response_set_date(http_response response, time_t value)
{
    assert(NULL != response);
    general_header_set_date(response->general_header, value);
}

int response_set_pragma(http_response response, const char* value)
{
    assert(NULL != response);
    return general_header_set_pragma(response->general_header, value);
}

int response_set_trailer(http_response response, const char* value)
{
    assert(NULL != response);
    return general_header_set_trailer(response->general_header, value);
}

int response_set_transfer_encoding(http_response response, const char* value)
{
    assert(NULL != response);
    return general_header_set_transfer_encoding(response->general_header, value);
}

void response_set_cachecontrol_public(http_response response)
{
    assert(response != NULL);
    general_header_set_public(response->general_header);
}

void response_set_cachecontrol_private(http_response response)
{
    assert(response != NULL);
    general_header_set_private(response->general_header);
}

void response_set_cachecontrol_no_cache(http_response response)
{
    assert(response != NULL);
    general_header_set_no_cache(response->general_header);
}

void response_set_cachecontrol_no_store(http_response response)
{
    assert(response != NULL);
    general_header_set_no_store(response->general_header);
}

void response_set_cachecontrol_no_transform(http_response response)
{
    assert(response != NULL);
    general_header_set_no_transform(response->general_header);
}

void response_set_cachecontrol_must_revalidate(http_response response)
{
    assert(response != NULL);
    general_header_set_must_revalidate(response->general_header);
}

void response_set_cachecontrol_proxy_revalidate(http_response response)
{
    assert(response != NULL);
    general_header_set_proxy_revalidate(response->general_header);
}

void response_set_cachecontrol_max_age(http_response response, int value)
{
    assert(response != NULL);
    general_header_set_max_age(response->general_header, value);
}

void response_set_cachecontrol_s_maxage(http_response response, int value)
{
    assert(response != NULL);
    general_header_set_s_maxage(response->general_header, value);
}

int response_set_upgrade(http_response response, const char* value)
{
    assert(NULL != response);
    return general_header_set_upgrade(response->general_header, value);
}

int response_set_via(http_response response, const char* value)
{
    assert(NULL != response);
    return general_header_set_via(response->general_header, value);
}

int response_set_warning(http_response response, const char* value)
{
    assert(NULL != response);
    return general_header_set_warning(response->general_header, value);
}

void response_set_accept_ranges(http_response response, int value)
{
    assert(NULL != response);
    assert(value == 0 || value == 1); /* send "none" or send "bytes" */

    response->accept_ranges = value;
    response_set_flag(response, ACCEPT_RANGES);
}

int response_set_etag(http_response response, const char* value)
{
    assert(NULL != response);
    assert(NULL != value);

    if (!cstring_copy(response->etag, value))
        return 0;

    response_set_flag(response, ETAG);
    return 1;
}

int response_set_location(http_response response, const char* value)
{
    assert(NULL != response);
    assert(NULL != value);

    if (!cstring_copy(response->location, value))
        return 0;

    response_set_flag(response, LOCATION);
    return 1;
}

int response_set_proxy_authenticate(http_response response, const char* value)
{
    assert(NULL != response);
    assert(NULL != value);

    if (!cstring_copy(response->proxy_authenticate, value))
        return 0;

    response_set_flag(response, PROXY_AUTHENTICATE);
    return 1;
}

int response_set_retry_after(http_response response, time_t value)
{
    assert(NULL != response);
    assert(value);

    response->retry_after = value;
    response_set_flag(response, RETRY_AFTER);
    return 1;
}

int response_set_server(http_response response, const char* value)
{
    assert(NULL != response);
    assert(NULL != value);

    if (!cstring_copy(response->server, value))
        return 0;

    response_set_flag(response, SERVER);
    return 1;
}

int response_set_vary(http_response response, const char* value)
{
    assert(NULL != response);
    assert(NULL != value);

    if (!cstring_copy(response->vary, value))
        return 0;

    response_set_flag(response, VARY);
    return 1;
}

int response_set_www_authenticate(http_response response, const char* value)
{
    assert(NULL != response);
    assert(NULL != value);

    if (!cstring_copy(response->www_authenticate, value))
        return 0;

    response_set_flag(response, WWW_AUTHENTICATE);
    return 1;
}

int response_set_allow(http_response response, const char* value)
{
    assert(NULL != response);
    return entity_header_set_allow(response->entity_header, value);
}

int response_set_content_encoding(http_response response, const char* value)
{
    assert(NULL != response);
    return entity_header_set_content_encoding(response->entity_header, value);
}

int response_set_content_language(http_response response, const char* value)
{
    assert(NULL != response);
    return entity_header_set_content_language(response->entity_header, value);
}

int response_set_content_length(http_response response, size_t value)
{
    assert(NULL != response);
    entity_header_set_content_length(response->entity_header, value);
    return 1;
}

int response_set_content_location(http_response response, const char* value)
{
    assert(NULL != response);
    return entity_header_set_content_location(response->entity_header, value);
}

int response_set_content_md5(http_response response, const char* value)
{
    assert(NULL != response);
    return entity_header_set_content_md5(response->entity_header, value);
}

int response_set_content_range(http_response response, const char* value)
{
    assert(NULL != response);
    return entity_header_set_content_range(response->entity_header, value);
}

int response_set_content_type(http_response response, const char* value)
{
    assert(NULL != response);
    return entity_header_set_content_type(response->entity_header, value);
}

void response_set_expires(http_response response, time_t value)
{
    assert(NULL != response);
    entity_header_set_expires(response->entity_header, value);
}

void response_set_last_modified(http_response response, time_t value)
{
    assert(NULL != response);
    entity_header_set_last_modified(response->entity_header, value);
}

void response_recycle(http_response p)
{
    assert(NULL != p);

    /* Free cookies */
    if (p->cookies) {
        list_free(p->cookies, (void(*)(void*))cookie_free);
        p->cookies = list_new();
    }

    general_header_recycle(p->general_header);
    entity_header_recycle(p->entity_header);
    cstring_recycle(p->entity);
    cstring_recycle(p->path);
    response_clear_flags(p);
    response_set_content_type(p, "text/html");
    #if 0
    if (p->content_buffer_in_use && p->content_free_when_done) {
        free(p->content_buffer);
        p->content_buffer = 0;
    }
    #endif
    p->content_buffer_in_use = 0;
    p->content_free_when_done = 0;
    p->send_file = 0;

    cstring_recycle(p->etag);
    cstring_recycle(p->location);
    cstring_recycle(p->proxy_authenticate);
    cstring_recycle(p->server);
    cstring_recycle(p->vary);
    cstring_recycle(p->www_authenticate);
}

static void response_set_flag(http_response p, unsigned long flag)
{
    p->flags |= flag;
}

static int response_flag_isset(http_response p, unsigned long flag)
{
    return (p->flags & flag) ? 1 : 0;
}

static void response_clear_flags(http_response p)
{
    p->flags = 0;
}

int response_set_content_buffer(http_response p, void* data, size_t cb)
{
    response_set_flag(p, CONTENT_LENGTH);
    p->content_buffer_in_use = 1;
    p->content_buffer = data;
    response_set_content_length(p, cb);
    return 1;
}

int response_set_allocated_content_buffer(
    http_response p,
    void* src,
    size_t n)
{
    response_set_flag(p, CONTENT_LENGTH);
    p->content_free_when_done = 1;
    p->content_buffer_in_use = 1;
    p->content_buffer = src;
    response_set_content_length(p, n);
    return 1;
}


/*
 * How do we quote? We use ' in version 1.
 * How about ' in the value. Do we escape them or do
 * we double-quote them? (\' or '' ) I have to guess here,
 * since rfc2109 is VERY silent on this issue. We go for \'
 * since most browsers/servers are written in C and
 * C programmers tend to escape stuff.
 */
static int strcat_quoted(cstring dest, const char* s)
{
    assert(NULL != s);
    assert(NULL != dest);

    if (!cstring_charcat(dest, '\''))
        return 0;

    while (*s) {
        if (*s == '\'') {
            if (!cstring_charcat(dest, '\\'))
                return 0;
        }

        if (!cstring_charcat(dest, *s))
            return 0;

        s++;
    }

    return cstring_charcat(dest, '\'');
}

int response_send_file(http_response p, const char *path, const char* ctype, meta_error e)
{
    struct stat st;
    assert(p != NULL);
    assert(path != NULL);
    assert(ctype != NULL);

    if (stat(path, &st))
        return 0;

    if (!response_set_content_type(p, ctype))
        return set_os_error(e, errno);

    response_set_content_length(p, (size_t)st.st_size);
    if (cstring_copy(p->path, path)) {
        p->send_file = 1;
        return 1;
    }
    else
        return set_os_error(e, errno);
}

/**
 * Send the entire contents of a file to the client.
 * Note that we manually call connection_flush(). This is done so that
 * we won't run out of retry attempts when sending big files.
 */
static int send_entire_file(connection conn, const char *path, size_t *pcb)
{
    int fd, success = 1;
    char buf[8192];
    ssize_t cbRead;

    if ((fd = open(path, O_RDONLY)) == -1)
        return 0;

// Note that sendfile() usage must change when using ssl. (probably)
// boa 20130204
#if 0
#ifdef HAVE_SENDFILE
    {
        off_t offset = 0;
        int fd_out = connection_get_fd(conn);
        struct stat st;
        if (fstat(fd, &st) == -1) {
            close(fd);
            return 0;
        }

        connection_flush(conn);
        if (sendfile(fd_out, fd, &offset, st.st_size) == -1) {
            if (errno == EINVAL || errno == ENOSYS) { /* see man page for sendfile */
                goto fallback;
            }

            close(fd);
            return 0;
        }
        else {
            close(fd);
            return 1;
        }
    }

fallback:

#endif
#endif

    *pcb = 0;
    while ((cbRead = read(fd, buf, sizeof buf)) > 0) {
        *pcb += (size_t)cbRead;
        if (!connection_write(conn, buf, (size_t)cbRead))
            success = 0;
        else if(!connection_flush(conn))
            success = 0;

        if (!success)
            break;
    }

    close(fd);
    return success;
}


static int
response_send_entity(http_response r, connection conn, size_t *pcb)
{
    int success = 1;
    if (r->content_buffer_in_use) {
        size_t cb = response_get_content_length(r);
        *pcb = cb;
        if (cb > (64*1024)) {
            int timeout = 1;
            int retries = cb / 1024;

            success = connection_write_big_buffer(
                conn,
                r->content_buffer,
                cb,
                timeout,
                retries);
        }
        else
            success = connection_write(conn, r->content_buffer, cb);

        if (r->content_free_when_done) {
            free((void*)r->content_buffer);
            r->content_buffer = NULL;
        }

        if (!success)
            return 0;
    }
    else if(r->send_file) {
        if (!send_entire_file(conn, c_str(r->path), pcb))
            return 0;
    }
    else {
        const char* entity = response_get_entity(r);
        *pcb = response_get_content_length(r);
        if (!connection_write(conn, entity, *pcb))
            return 0;
    }

    return 1;
}

#if 0
static int write_406(connection conn, http_version v)
{
    int code = 406;

    /* http 1.0 has no 406, so we send a 400 instead */
    if (v != VERSION_11)
        code = 400;

    return send_status_code(conn, code, v);
}

/**
 * It is meaningful for some status codes to send content, not
 * so meaningful for others. This function tests to see if
 * we should send the content buffer or not.
 */
static int http_send_content(int status)
{
    switch (status) {
        case HTTP_200_OK:
        case HTTP_400_BAD_REQUEST:
        case HTTP_402_PAYMENT_REQUIRED:
        case HTTP_403_FORBIDDEN:
        case HTTP_404_NOT_FOUND:
        case HTTP_405_METHOD_NOT_ALLOWED:
        case HTTP_406_NOT_ACCEPTABLE:
            return 1;
        default:
            return 0;
    }
}
#endif

/* Returns 0 and sets e to the proper HTTP error code if a http error was sent
 * back to the user. Returns a tcpip_error in e if a tcp/ip error occurs, even
 * if the response originally was a http error. This is done so that we can
 * detect and handle disconnects or other tcp/ip issues when sending responses
 * back to the client.
 * Also note that this function will write to the logfile since this function
 * is the only function able to tell the size of the returned data effectively.
 *
 * First we send the HTTP status code, then the HTTP header fields, and last
 * but not least, the entity itself.
 */
size_t response_send(http_response r, connection c, meta_error e)
{
    int success = 0;
    size_t cb = 0;

    /* NOTE: 20060103
     * I have rewritten lots of stuff today and added general_header and entity_header
     * members. Due to that change, we need to double check a few things here, before
     * we start sending data to the client. First of all we must set the correct
     * content_length in the entity_header.
     */
    if (!entity_header_content_length_isset(r->entity_header)) {
        /* Shot in the dark, will not work for static pages */
        entity_header_set_content_length(r->entity_header, cstring_length(r->entity));
    }


    if (!send_status_code(c, r->status, r->version))
        set_tcpip_error(e, errno);
    else if(r->status != HTTP_200_OK && r->status != HTTP_404_NOT_FOUND) {
        /* NOTE: Fix this later. Other statuses than 200 should also return
         * headers and content. It's done this way now since I'm in the
         * middle of a rather big rewrite...
         * boa, 2003-11-26
         */

        /* NOTE 2, 2003-12-15: OK, Now is the time to fix this, but WTF was
         * I supposed to do here? I cannot recall what the problem was. :-(
         * I assume that it was related to the fact that some status codes
         * implies that the entity is sent along with the status code,
         * and other status codes do not send the entity. Not that *I*
         * currently send anything but 200 and 404,
         * but even 404 implies a body.Hmm...
         * I've added the line below as a Q&D fix.
         */
        response_send_header(r, c, e);
    }
    else if(!response_send_header(r, c, e))
        ;
    else if(!response_send_entity(r, c, &cb))
        set_tcpip_error(e, errno);
    else
        success = 1;

    return success;
}

const char* response_get_connection(http_response response)
{
    assert(response != NULL);
    return general_header_get_connection(response->general_header);
}

int response_td(http_response page, const char* text)
{
    return cstring_concat3(page->entity, "<td>", text, "</td>\n");
}

int response_br(http_response response)
{
    assert(response != NULL);
    return cstring_concat(response->entity, "<br>");
}

int response_hr(http_response response)
{
    assert(response != NULL);
    return cstring_concat(response->entity, "<hr>");
}

int response_href(http_response response, const char* ref, const char* text)
{
    size_t cb;
    const char* fmt = "<a href=\"%s\">%s</a>";

    assert(response != NULL);
    assert(ref != NULL);
    assert(text != NULL);

    /* We need cb bytes, more or less. %s*2 == 4 bytes more than we need,
     * + 1 for the null character. This is therefore OK.
     */
    cb = strlen(ref) + strlen(text) + strlen(fmt);
    return cstring_printf(response->entity, cb, fmt, ref, text);
}

int response_p (http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<p>", s, "</p>\n");
}

int response_h1(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h1>", s, "</h1>\n");
}

int response_h2(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h2>", s, "</h2>\n");
}

int response_h3(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h3>", s, "</h3>\n");
}

int response_h4(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h4>", s, "</h4>\n");
}

int response_h5(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h5>", s, "</h5>\n");
}

int response_h6(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h6>", s, "</h6>\n");
}

int response_h7(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h7>", s, "</h7>\n");
}

int response_h8(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h8>", s, "</h8>\n");
}

int response_h9(http_response response, const char* s)
{
    assert(response != NULL);
    assert(s != NULL);

    assert(s != NULL);

    return cstring_concat3(response->entity, "<h9>", s, "</h9>\n");
}

int response_js_messagebox(http_response response, const char* text)
{
    const char* start = "<script language=\"javascript\">\nalert(\"";
    const char* end = "\");\n</script>\n";

    assert(response != NULL);
    assert(text != NULL);

    return cstring_concat3(response->entity, start, text, end);
}

/* response field functions */
static int parse_accept_ranges(http_response r, const char* value, meta_error e);
static int parse_age(http_response r, const char* value, meta_error e);
static int parse_etag(http_response r, const char* value, meta_error e);
static int parse_location(http_response r, const char* value, meta_error e);
static int parse_proxy_authenticate(http_response r, const char* value, meta_error e);
static int parse_retry_after(http_response r, const char* value, meta_error e);
static int parse_server(http_response r, const char* value, meta_error e);
static int parse_vary(http_response r, const char* value, meta_error e);
static int parse_www_authenticate(http_response r, const char* value, meta_error e);

static const struct response_mapper {
    const char* name;
    int (*handler)(http_response req, const char* value, meta_error e);
} response_header_fields[] = {
    { "accept-ranges", parse_accept_ranges },
    { "age", parse_age },
    { "etag", parse_etag },
    { "location", parse_location },
    { "proxy-authenticate", parse_proxy_authenticate },
    { "retry-after", parse_retry_after },
    { "server", parse_server },
    { "vary", parse_vary },
    { "www-authenticate", parse_www_authenticate }
};

int find_response_header(const char* name)
{
    int i, nelem = sizeof response_header_fields / sizeof *response_header_fields;
    for (i = 0; i < nelem; i++) {
        if (strcmp(response_header_fields[i].name, name) == 0)
            return i;
    }

    return -1;
}

int parse_response_header(int idx, http_response req, const char* value, meta_error e)
{
    assert(idx >= 0);
    assert((size_t)idx < sizeof response_header_fields / sizeof *response_header_fields);

    return response_header_fields[idx].handler(req, value, e);
}

static int parse_age(http_response r, const char* value, meta_error e)
{
    unsigned long v;
    assert(r != NULL);
    assert(value != NULL);

    if ((v = strtoul(value, NULL, 10)) == 0)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    response_set_age(r, v);
    return 1;
}

static int parse_etag(http_response r, const char* value, meta_error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_etag(r, value))
        return set_os_error(e, errno);

    return 1;
}

static int parse_location(http_response r, const char* value, meta_error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_location(r, value))
        return set_os_error(e, errno);

    return 1;
}

/* §14.5: Accept-Ranges is either "bytes", "none", or range-units(section 3.12)
 * The only range unit defined by HTTP 1.1 is "bytes", and we MAY ignore all others.
 */
static int parse_accept_ranges(http_response r, const char* value, meta_error e)
{
    assert(r != NULL);
    assert(value != NULL);

    (void)e;

    if (strcmp(value, "bytes") == 0)
        response_set_accept_ranges(r, 1);
    else if(strcmp(value, "none") == 0)
        response_set_accept_ranges(r, 0);


    /* Silently ignore other range units */
    return 1;
}

static int parse_proxy_authenticate(http_response r, const char* value, meta_error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_proxy_authenticate(r, value))
        return set_os_error(e, errno);

    return 1;
}

/*
 * The value can be either a rfc822 date or an integer value representing delta(seconds).
 * TODO: We need a way to separate between delta and absolute time.
 */
static int parse_retry_after(http_response r, const char* value, meta_error e)
{
    time_t t;
    long delta;

    assert(r != NULL);
    assert(value != NULL);

    if ((t = parse_rfc822_date(value)) != (time_t)-1)
        response_set_retry_after(r, t);
    else if((delta = atol(value)) <= 0)
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    else
        response_set_retry_after(r, delta);

    return 1;
}

static int parse_vary(http_response r, const char* value, meta_error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_vary(r, value))
        return set_os_error(e, errno);
    return 1;
}

static int parse_www_authenticate(http_response r, const char* value, meta_error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_www_authenticate(r, value))
        return set_os_error(e, errno);
    return 1;
}

static int parse_server(http_response r, const char* value, meta_error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_server(r, value))
        return set_os_error(e, errno);

    return 1;
}

/* The response status line(§6.1) is
 *      HTTP-Version SP Status-Code SP Reason-Phrase CRLF
 * It is the first line in all HTTP responses.
 */
static int read_response_status_line(http_response response, connection conn, meta_error e)
{
    char *s, buf[CCH_STATUSLINE_MAX + 1];
    int status_code;
    enum http_version version;

    if (!read_line(conn, buf, sizeof(buf) - 1, e))
        return 0;

    /* The string must start with either HTTP/1.0 or HTTP/1.1 SP */
    if (strstr(buf, "HTTP/1.0 ") == buf) {
        version = VERSION_10;
    }
    else if(strstr(buf, "HTTP/1.1 ") == buf) {
        version = VERSION_11;
    }
    else {
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    /* Skip version and first SP */
    s = buf + 9;

    /* Double check that we still have the right format.
     * s[0], s[1] and s[2] MUST be a digit, s[3] MUST be a space
     */
    if (!isdigit((int)s[0])
    || !isdigit((int)s[1])
    || !isdigit((int)s[2])
    || !isspace((int)s[3]))
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    status_code
        = (s[0] - '0') * 100
        + (s[1] - '0') * 10
        + (s[2] - '0') * 1;

    /* Skip status code and SP */
    s += 4;

    /* We need the reason-phrase */
    if (*s == '\0')
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    response_set_status(response, status_code);
    response_set_version(response, version);
    return 1;
}

/* Reads all (if any) http header fields */
static int
read_response_header_fields(connection conn, http_response response, meta_error e)
{
    for (;;) {
        char buf[CCH_FIELDNAME_MAX + CCH_FIELDVALUE_MAX + 10];
        char name[CCH_FIELDNAME_MAX + 1];
        char value[CCH_FIELDVALUE_MAX + 1];

        if ((!read_line(conn, buf, sizeof buf, e))) {
            return 0;
        }
        else if(strlen(buf) == 0) {
            /*
             * An empty buffer means that we have read the \r\n sequence
             * separating header fields from entities or terminating the message.
             * This means that there is no more header fields to read.
             */
            fprintf(stderr, "No more header fields!\n");
            return 1;
        }

        if (!get_field_name(buf, name, CCH_FIELDNAME_MAX)
        || !get_field_value(buf, value, CCH_FIELDVALUE_MAX)) {
            return set_http_error(e, HTTP_400_BAD_REQUEST);
        }

        fs_lower(name);
        if (!parse_response_headerfield(name, value, response, e))
            return 0;
    }
}

int response_receive(http_response response, connection conn, size_t max_content, meta_error e)
{
    entity_header eh = response_get_entity_header(response);
    size_t content_length;
    void *content;

    if (!read_response_status_line(response, conn, e)) {
        return 0;
    }

    /* Now read and parse all fields (if any) */
    if (!read_response_header_fields(conn, response, e)) {
        return 0;
    }

    /* Now we hopefully have a content-length field. See if we can read
     * it or if it is too big
     */
    if (!entity_header_content_length_isset(eh)) {
        /* No content length, then we MUST deal with a version 1.0 server.
         * Read until max_content is reached or socket is closed.
         */
        content_length = max_content;
        if ((content = malloc(content_length)) == NULL)
            return set_os_error(e, errno);
        else if(!connection_read(conn, content, max_content)) {
            set_os_error(e, errno);
            free(content);
            return 0;
        }
        else if(!response_set_allocated_content_buffer(response, content, content_length)) {
            set_os_error(e, errno);
            free(content);
            return 0;
        }
        else {
            return 1;
        }
    }
    else {
        content_length = entity_header_get_content_length(eh);
        if (content_length == 0)
            return 1;
        else if(content_length > max_content)
            return set_app_error(e, ENOSPC);
        else if((content = malloc(content_length)) == NULL)
            return set_os_error(e, errno);
        else if(!connection_read(conn, content, content_length)) {
            set_os_error(e, errno);
            free(content);
            return 0;
        }
        else if(!response_set_allocated_content_buffer(response, content, content_length)) {
            set_os_error(e, errno);
            free(content);
            return 0;
        }
        else {
            return 1;
        }
    }
}

int response_dump(http_response r, FILE *f)
{

    /* Now our own fields */
    const char* version = "Unknown";
    switch (r->version) {
        case VERSION_09:
            version = "HTTP 0.9";
            break;

        case VERSION_10:
            version = "HTTP/1.0";
            break;

        case VERSION_11:
            version = "HTTP/1.1";
            break;

        case VERSION_UNKNOWN:
        default:
            version = "Unknown";
            break;
    }

    fprintf(f, "Version: %s\n", version);
    fprintf(f, "Status-Code: %d\n", r->status);

    if (response_flag_isset(r, AGE))
        fprintf(f, "Age: %lu\n", r->age);

    general_header_dump(r->general_header, f);
    entity_header_dump(r->entity_header, f);

    if (response_flag_isset(r, ACCEPT_RANGES))
        fprintf(f, "Accept-Ranges: %d\n", r->accept_ranges);

    if (response_flag_isset(r, ETAG))
        fprintf(f, "ETag: %s\n", c_str(r->etag));

    if (response_flag_isset(r, LOCATION))
        fprintf(f, "Location: %s\n", c_str(r->location));

    if (response_flag_isset(r, PROXY_AUTHENTICATE))
        fprintf(f, "Proxy-Authenticate: %s\n", c_str(r->proxy_authenticate));

    if (response_flag_isset(r, RETRY_AFTER))
        fprintf(f, "Retry-After: %lu\n", (unsigned long)r->retry_after);

    if (response_flag_isset(r, SERVER))
        fprintf(f, "Server: %s\n", c_str(r->server));

    if (response_flag_isset(r, VARY))
        fprintf(f, "Vary: %s\n", c_str(r->vary));

    if (response_flag_isset(r, WWW_AUTHENTICATE))
        fprintf(f, "WWW-Authenticate: %s\n", c_str(r->www_authenticate));

    list_iterator li;
    for (li = list_first(r->cookies); !list_end(li); li = list_next(li)) {
        cookie c = list_get(li);
        cookie_dump(c, f);
    }

    return 1;
}
