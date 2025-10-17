/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <meta_list.h>
#include <meta_misc.h>
#include <meta_convert.h>

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

    /* The page function can assign its own content buffer.	 */
    void* content_buffer;
    int content_buffer_in_use;
    int content_free_when_done;

    /* sometimes we want to send an entire file instead of
     * regular content. */
    int send_file;
    cstring path;
};

static void response_set_flag(http_response p, unsigned long flag)
{
    p->flags |= flag;
}

static bool response_flag_isset(http_response p, unsigned long flag)
{
    return p->flags & flag;
}

static void response_clear_flags(http_response p)
{
    p->flags = 0;
}

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
    assert(p != NULL);

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
    cstring strings[8];
    size_t nstrings = sizeof strings / sizeof *strings;

    if ((p = calloc(1, sizeof *p)) == NULL || !cstring_multinew(strings, nstrings)) {
        free(p);
        return NULL;
    }

    if ((p->general_header = general_header_new()) == NULL
    ||	(p->entity_header = entity_header_new()) == NULL) {
        general_header_free(p->general_header);
        cstring_multifree(strings, nstrings);
        free(p);
        return NULL;
    }

    if ((p->cookies = list_new()) == NULL) {
        general_header_free(p->general_header);
        entity_header_free(p->entity_header);
        cstring_multifree(strings, nstrings);
        free(p);
        return NULL;
    }

    p->version = VERSION_UNKNOWN;
    p->status = 0;
    p->flags = 0;
    p->retry_after = (time_t)-1;
    p->send_file = 0;

    p->etag = strings[0];
    p->location = strings[1];
    p->proxy_authenticate = strings[2];
    p->server = strings[3];
    p->vary = strings[4];
    p->www_authenticate = strings[5];
    p->entity = strings[6];
    p->path = strings[7];

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

static status_t send_age(connection c, http_response p)
{
    return http_send_ulong(c, "Age: ", p->age);
}

static status_t send_etag(connection conn, http_response p)
{
    return http_send_field(conn, "ETag: ", p->etag);
}

static status_t send_location(connection conn, http_response p)
{
    return http_send_field(conn, "Location: ", p->location);
}

static status_t send_proxy_authenticate(connection conn, http_response p)
{
    return http_send_field(conn, "Proxy-Authenticate: ", p->proxy_authenticate);
}

static status_t send_server(connection conn, http_response p)
{
    return http_send_field(conn, "Server: ", p->server);
}

static status_t send_vary(connection conn, http_response p)
{
    return http_send_field(conn, "Vary: ", p->vary);
}

static status_t send_www_authenticate(connection conn, http_response p)
{
    return http_send_field(conn, "WWW-Authenticate: ", p->www_authenticate);
}


static status_t send_retry_after(connection conn, http_response p)
{
    return http_send_date(conn, "Retry-After: ", p->retry_after);
}

static status_t send_accept_ranges(connection conn, http_response p)
{
    size_t cch;
    const char *s;

    if (p->accept_ranges == 1)
        s = "Accept-Ranges: bytes\r\n";
    else
        s = "Accept-Ranges: none\r\n";

    cch = strlen(s);
    return connection_write(conn, s, cch);
}


static status_t response_send_header_fields(http_response p, connection conn)
{
    status_t status = success;
    size_t i, n;

    static const struct {
        size_t flag;
        status_t (*func)(connection, http_response);
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

    assert(p != NULL);
    assert(conn != NULL);

    /*
     * Some fields are required by http. We add them if the
     * user hasn't added them manually.
     */
    if (!general_header_date_isset(p->general_header))
        general_header_set_date(p->general_header, time(NULL));

    if (!general_header_send_fields(p->general_header, conn)
    ||  !entity_header_send_fields(p->entity_header, conn))
        return failure;


    if (status) {
        n = sizeof fields / sizeof *fields;
        for (i = 0; i < n; i++) {
            if (response_flag_isset(p, fields[i].flag)) {
                status = (*fields[i].func)(conn, p);
                if (!status)
                    break;
            }
        }
    }

    return status;
}

/* return 1 if string needs to be quoted, 0 if not */
static bool need_quote(const char *s)
{
    while (*s != '\0') {
        if (!isalnum(*s) && *s != '_')
            return true;

        s++;
    }

    return false;
}

/*
 * How do we quote? We use ' in version 1.  What about ' in the value? Do we
 * escape them or do we double-quote them? (\' or '' ) I have to guess here,
 * since rfc2109 is VERY silent on this issue. We go for \' since most
 * browsers/servers are written in C and C programmers tend to escape stuff.
 */
static status_t strcat_quoted(cstring dest, const char *s)
{
    assert(s != NULL);
    assert(dest != NULL);

    if (!cstring_charcat(dest, '\''))
        return failure;

    while (*s) {
        if (*s == '\'') {
            if (!cstring_charcat(dest, '\\'))
                return failure;
        }

        if (!cstring_charcat(dest, *s))
            return failure;

        s++;
    }

    return cstring_charcat(dest, '\'');
}

/*
 * Creates a string consisting of the misc elements in a cookie.
 * String must have been new'ed.
 */
static status_t create_cookie_string(cookie c, cstring str)
{
    const char *s;
    int v;

    if ((s = cookie_get_name(c)) == NULL)
        return failure;

    if (!cstring_set(str, "Set-Cookie: "))
        return failure;

    if (!cstring_concat(str, s))
        return failure;

    /* Now get value and append. Remember to quote value if needed
     * NOTE: Netscape chokes, acc. to rfc2109, on quotes.
     * We therefore need to know the version and at least "quote when needed"
     */
    s = cookie_get_value(c);
    if (s) {
        if (!cstring_charcat(str, '='))
            return failure;

        if (need_quote(c_str(str)) && !strcat_quoted(str, s))
            return failure;

        if (!cstring_concat(str, s))
            return failure;
    }

    v = cookie_get_version(c);
    if (!cstring_printf(str, ";Version=%d", v))
        return failure;

    v = cookie_get_max_age(c);
    if (v != MAX_AGE_NOT_SET && !cstring_printf(str, ";Max-Age=%d", v))
        return failure;

    v = cookie_get_secure(c);
    if (!cstring_printf(str, ";Secure=%d", v))
        return failure;

    s = cookie_get_domain(c);
    if (s != NULL && !cstring_concat2(str, ";Domain=", s))
        return failure;

    s = cookie_get_comment(c);
    if (s != NULL && !cstring_concat2(str, ";Comment=", s))
        return failure;

    s = cookie_get_path(c);
    if (s != NULL && !cstring_concat2(str, ";Path=", s))
        return failure;

    return cstring_concat(str, "\r\n");
}

static status_t send_cookie(cookie c, connection conn, error e)
{
    cstring s;
    size_t count;

    assert(c != NULL);
    assert(conn != NULL);

    if ((s = cstring_new()) == NULL)
        return set_os_error(e, ENOMEM);

    if (!create_cookie_string(c, s)) {
        cstring_free(s);
        return set_os_error(e, ENOMEM);
    }

    count = cstring_length(s);
    if (!connection_write(conn, c_str(s), count)) {
        set_tcpip_error(e, errno);
        cstring_free(s);
        return failure;
    }

    cstring_free(s);
    return success;
}

static status_t response_send_cookies(http_response p, connection conn,
    error e)
{
    list_iterator i;

    assert(p != NULL);
    assert(conn != NULL);

    for (i = list_first(p->cookies); !list_end(i); i = list_next(i)) {
        cookie c;

        c = list_get(i);
        if (!send_cookie(c, conn, e))
            return failure;
    }

    return success;
}

static status_t response_send_header(http_response response,
    connection conn, error e)
{
    if (response->version == VERSION_09)
        /* No headers for http 0.9 */
        return failure;

    /* Special stuff to support pers. conns in HTTP 1.0 */
    if (connection_is_persistent(conn)
    && response->version == VERSION_10
    && !response_set_connection(response, "Keep-Alive"))
        return set_os_error(e, errno);

    if (!response_send_header_fields(response, conn))
        return set_tcpip_error(e, errno);

    /* Send cookies, if any */
    if (response->cookies != NULL
    && !response_send_cookies(response, conn, e))
        return failure;

    /* Send the \r\n separating all headers from an optional entity */
    if (!connection_write(conn, "\r\n", 2))
        return set_tcpip_error(e, errno);

    return success;
}

status_t response_add(const http_response p, const char *value)
{
    assert(p != NULL);
    assert(value != NULL);

    return cstring_concat(p->entity, value);
}

status_t response_add_char(http_response p, int c)
{
    assert(p != NULL);
    return cstring_charcat(p->entity, c);
}

status_t response_add_end(http_response response, const char *start, const char *end)
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

        if (p->content_free_when_done)
            free(p->content_buffer);

        free(p);
    }
}

status_t response_printf(http_response page, const char *fmt, ...)
{
    status_t status;
    va_list ap;

    assert(page != NULL);
    assert(fmt != NULL);

    va_start (ap, fmt);
    status = cstring_vprintf(page->entity, fmt, ap);
    va_end(ap);

    return status;
}


status_t response_set_cookie(http_response response, cookie new_cookie)
{
    list_iterator i;
    const char *nameNew;

    nameNew = cookie_get_name(new_cookie);

    /* Lookup cookie to check for duplicates */
    for (i = list_first(response->cookies); !list_end(i); i = list_next(i)) {
        const char *nameOld;
        cookie c;

        c = list_get(i);
        assert(c != NULL);
        nameOld = cookie_get_name(c);
        if (0 == strcmp(nameNew, nameOld))
            return fail(EINVAL); /* We have a duplicate */
    }

    if (!list_add(response->cookies, new_cookie))
        return failure;

    return success;
}

status_t http_send_date(connection conn, const char *name, time_t value)
{
    char date[100];
    size_t cb;
    struct tm t, *ptm = &t;

    assert(name != NULL);

    cb = strlen(name);
    if (connection_write(conn, name, cb)) {
        gmtime_r(&value, ptm);
        cb = strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT\r\n", ptm);
        return connection_write(conn, date, cb);
    }

    return failure;
}

status_t http_send_string(connection conn, const char *s)
{
    assert(s != NULL);

    size_t cb = strlen(s);
    return connection_write(conn, s, cb);
}

status_t http_send_ulong(connection conn, const char *name, unsigned long value)
{
    char val[1000];
    size_t cb = snprintf(val, sizeof val, "%s%lu", name, value);
    if (cb >= sizeof val)
        return failure;

    return connection_write(conn, val, cb);
}

status_t http_send_field(connection conn, const char *name, cstring value)
{
    size_t cb;

    assert(conn != NULL);
    assert(name != NULL);
    assert(value != NULL);

    cb = strlen(name);
    if (!connection_write(conn, name, cb))
        return failure;

    cb = cstring_length(value);
    if (!connection_write(conn, c_str(value), cb))
        return failure;

    return connection_write(conn, "\r\n", 2);
}

size_t response_get_content_length(http_response p)
{
    size_t contentlen = 0;

    assert(p != NULL);
    assert(p->entity != NULL);

    if (entity_header_content_length_isset(p->entity_header)) {
        contentlen = entity_header_get_content_length(p->entity_header);
    }
    else {
        /* Shot in the dark, will not work for static pages */
        contentlen = cstring_length(p->entity);
    }

    return contentlen;
}


status_t response_set_connection(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    return general_header_set_connection(response->general_header, value);
}

void response_set_date(http_response response, time_t value)
{
    assert(response != NULL);
    general_header_set_date(response->general_header, value);
}

status_t response_set_pragma(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    return general_header_set_pragma(response->general_header, value);
}

status_t response_set_trailer(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    return general_header_set_trailer(response->general_header, value);
}

status_t response_set_transfer_encoding(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

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

status_t response_set_upgrade(http_response response, const char *value)
{
    assert(response != NULL);
    return general_header_set_upgrade(response->general_header, value);
}

status_t response_set_via(http_response response, const char *value)
{
    assert(response != NULL);
    return general_header_set_via(response->general_header, value);
}

status_t response_set_warning(http_response response, const char *value)
{
    assert(response != NULL);
    return general_header_set_warning(response->general_header, value);
}

void response_set_accept_ranges(http_response response, int value)
{
    assert(response != NULL);
    assert(value == 0 || value == 1); /* send "none" or send "bytes" */

    response->accept_ranges = value;
    response_set_flag(response, ACCEPT_RANGES);
}

status_t response_set_etag(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    if (!cstring_set(response->etag, value))
        return failure;

    response_set_flag(response, ETAG);
    return success;
}

status_t response_set_location(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    if (!cstring_set(response->location, value))
        return failure;

    response_set_flag(response, LOCATION);
    return success;
}

status_t response_set_proxy_authenticate(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    if (!cstring_set(response->proxy_authenticate, value))
        return failure;

    response_set_flag(response, PROXY_AUTHENTICATE);
    return success;
}

void response_set_retry_after(http_response response, time_t value)
{
    assert(response != NULL);
    assert(value);

    response->retry_after = value;
    response_set_flag(response, RETRY_AFTER);
}

status_t response_set_server(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    if (!cstring_set(response->server, value))
        return failure;

    response_set_flag(response, SERVER);
    return success;
}

status_t response_set_vary(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    if (!cstring_set(response->vary, value))
        return failure;

    response_set_flag(response, VARY);
    return success;
}

status_t response_set_www_authenticate(http_response response, const char *value)
{
    assert(response != NULL);
    assert(value != NULL);

    if (!cstring_set(response->www_authenticate, value))
        return failure;

    response_set_flag(response, WWW_AUTHENTICATE);
    return success;
}

status_t response_set_allow(http_response response, const char *value)
{
    assert(response != NULL);
    return entity_header_set_allow(response->entity_header, value);
}

status_t response_set_content_encoding(http_response response, const char *value)
{
    assert(response != NULL);
    return entity_header_set_content_encoding(response->entity_header, value);
}

status_t response_set_content_language(http_response response, const char *value, error e)
{
    assert(response != NULL);
    return entity_header_set_content_language(response->entity_header, value, e);
}

void response_set_content_length(http_response response, size_t value)
{
    assert(response != NULL);
    entity_header_set_content_length(response->entity_header, value);
}

status_t response_set_content_location(http_response response, const char *value)
{
    assert(response != NULL);
    return entity_header_set_content_location(response->entity_header, value);
}

status_t response_set_content_md5(http_response response, const char *value)
{
    assert(response != NULL);
    return entity_header_set_content_md5(response->entity_header, value);
}

status_t response_set_content_range(http_response response, const char *value)
{
    assert(response != NULL);
    return entity_header_set_content_range(response->entity_header, value);
}

status_t response_set_content_type(http_response response, const char *value)
{
    assert(response != NULL);
    return entity_header_set_content_type(response->entity_header, value);
}

void response_set_expires(http_response response, time_t value)
{
    assert(response != NULL);
    entity_header_set_expires(response->entity_header, value);
}

void response_set_last_modified(http_response response, time_t value)
{
    assert(response != NULL);
    entity_header_set_last_modified(response->entity_header, value);
}

void response_recycle(http_response p)
{
    assert(p != NULL);

    /* Free cookies */
    if (p->cookies) {
        list_free(p->cookies, cookie_freev);
        p->cookies = list_new();
    }

    general_header_recycle(p->general_header);
    entity_header_recycle(p->entity_header);
    cstring_recycle(p->entity);
    cstring_recycle(p->path);
    response_clear_flags(p);
    if (!response_set_content_type(p, "text/html"))
        warning("Probably out of memory\n");

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

void response_set_content_buffer(http_response p, void* data, size_t cb)
{
    response_set_flag(p, CONTENT_LENGTH);
    p->content_buffer_in_use = 1;
    p->content_buffer = data;
    response_set_content_length(p, cb);
}

void response_set_allocated_content_buffer(
    http_response p,
    void* src,
    size_t n)
{
    response_set_flag(p, CONTENT_LENGTH);
    p->content_free_when_done = 1;
    p->content_buffer_in_use = 1;
    p->content_buffer = src;
    response_set_content_length(p, n);
}


status_t response_send_file(http_response p, const char *path, const char *ctype, error e)
{
    struct stat st;
    assert(p != NULL);
    assert(path != NULL);
    assert(ctype != NULL);

    if (stat(path, &st))
        return failure;

    if (!response_set_content_type(p, ctype))
        return set_os_error(e, errno);

    response_set_content_length(p, (size_t)st.st_size);
    if (cstring_set(p->path, path)) {
        p->send_file = 1;
        return success;
    }

    return set_os_error(e, errno);
}

/*
 * Send the entire contents of a file to the client.
 * Note that we manually call connection_flush(). This is done so that
 * we won't run out of retry attempts when sending big files.
 * (yes, I know sendfile() exists. See git log)
 *
 * BTW, maybe we want to mmap the file instead of reading it?
 */
static status_t send_entire_file(connection conn, const char *path, size_t *pcb)
{
    int fd;
    status_t retval = success;
    char buf[8192];
    ssize_t nread;

    if ((fd = open(path, O_RDONLY)) == -1)
        return failure;

    *pcb = 0;
    while ((nread = read(fd, buf, sizeof buf)) > 0) {
        *pcb += (size_t)nread;
        if (!connection_write(conn, buf, (size_t)nread))
            retval = failure;
        else if (!connection_flush(conn))
            retval = failure;

        if (!retval)
            break;
    }

    close(fd);
    return retval;
}

static status_t
response_send_entity(http_response r, connection conn, size_t *pcb)
{
    status_t rc = success;

    if (r->content_buffer_in_use) {
        size_t cb = response_get_content_length(r);
        *pcb = cb;
        if (cb > 64 * 1024) {
            int timeout = 1;
            int retries = cb / 1024;

            rc = connection_write_big_buffer(conn, r->content_buffer,
                cb, timeout, retries);
        }
        else
            rc = connection_write(conn, r->content_buffer, cb);

        if (r->content_free_when_done) {
            free(r->content_buffer);
            r->content_buffer = NULL;
        }

        if (!rc)
            return failure;
    }
    else if (r->send_file) {
        if (!send_entire_file(conn, c_str(r->path), pcb))
            return failure;
    }
    else {
        const char *entity = response_get_entity(r);
        *pcb = response_get_content_length(r);
        if (!connection_write(conn, entity, *pcb))
            return failure;
    }

    return success;
}

/*
 * Returns failure and sets e to the proper HTTP error code if a http error
 * was sent back to the user. Returns a tcpip_error in e if a tcp/ip error
 * occurs, even if the response originally was a http error.
 * This is done so that we can detect and handle disconnects or
 * other tcp/ip issues when sending responses back to the client.
 *
 * First we send the HTTP status code, then the HTTP header fields, and last
 * but not least, the entity itself.
 */
status_t response_send(http_response r, connection c, error e, size_t *pcb)
{
    /* NOTE: 20060103
     * I have rewritten lots of stuff today and added general_header
     * and entity_header members. Due to that change, we need to double
     * check a few things here, before we start sending data to the client.
     * First of all we must set the correct content_length in the
     * entity_header.
     */
    if (!entity_header_content_length_isset(r->entity_header)) {
        /* Shot in the dark, will not work for static pages */
        entity_header_set_content_length(r->entity_header, cstring_length(r->entity));
    }


    if (!send_status_code(c, r->status, r->version))
        return set_tcpip_error(e, errno);

    if (r->status != HTTP_200_OK && r->status != HTTP_404_NOT_FOUND) {
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
        return failure;
    }

    if (!response_send_header(r, c, e))
        return failure;

    if (!response_send_entity(r, c, pcb))
        return set_tcpip_error(e, errno);

    return success;
}

const char* response_get_connection(http_response response)
{
    assert(response != NULL);
    return general_header_get_connection(response->general_header);
}

status_t response_td(http_response page, const char *text)
{
    return cstring_concat3(page->entity, "<td>", text, "</td>\n");
}

status_t response_br(http_response response)
{
    assert(response != NULL);
    return cstring_concat(response->entity, "<br>");
}

status_t response_hr(http_response response)
{
    assert(response != NULL);
    return cstring_concat(response->entity, "<hr>");
}

status_t response_href(http_response response, const char *ref, const char *text)
{
    const char *fmt = "<a href=\"%s\">%s</a>";

    assert(response != NULL);
    assert(ref != NULL);
    assert(text != NULL);

    return cstring_printf(response->entity, fmt, ref, text);
}

status_t response_p (http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<p>", s, "</p>\n");
}

status_t response_h1(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h1>", s, "</h1>\n");
}

status_t response_h2(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h2>", s, "</h2>\n");
}

status_t response_h3(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h3>", s, "</h3>\n");
}

status_t response_h4(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h4>", s, "</h4>\n");
}

status_t response_h5(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h5>", s, "</h5>\n");
}

status_t response_h6(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h6>", s, "</h6>\n");
}

status_t response_h7(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h7>", s, "</h7>\n");
}

status_t response_h8(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    return cstring_concat3(response->entity, "<h8>", s, "</h8>\n");
}

status_t response_h9(http_response response, const char *s)
{
    assert(response != NULL);
    assert(s != NULL);

    assert(s != NULL);

    return cstring_concat3(response->entity, "<h9>", s, "</h9>\n");
}

status_t response_js_messagebox(http_response response, const char *text)
{
    const char *start = "<script language=\"javascript\">\nalert(\"";
    const char *end = "\");\n</script>\n";

    assert(response != NULL);
    assert(text != NULL);

    return cstring_concat3(response->entity, start, text, end);
}

/* response field functions */
static status_t parse_age(http_response r, const char *value, error e)
{
    unsigned long v;
    assert(r != NULL);
    assert(value != NULL);

    if (!toulong(value, &v))
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    response_set_age(r, v);
    return success;
}

static status_t parse_etag(http_response r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_etag(r, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_location(http_response r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_location(r, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_www_authenticate(http_response r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_www_authenticate(r, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_server(http_response r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_server(r, value))
        return set_os_error(e, errno);

    return success;
}

/*
 * §14.5: Accept-Ranges is either "bytes", "none", or range-units(section 3.12)
 * The only range unit defined by HTTP 1.1 is "bytes", and we MAY ignore
 * all others.
 */
static status_t parse_accept_ranges(http_response r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    (void)e;

    if (strcmp(value, "bytes") == 0)
        response_set_accept_ranges(r, 1);
    else if (strcmp(value, "none") == 0)
        response_set_accept_ranges(r, 0);


    /* Silently ignore other range units */
    return success;
}

static status_t parse_proxy_authenticate(http_response r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_proxy_authenticate(r, value))
        return set_os_error(e, errno);

    return success;
}

/*
 * The value can be either a rfc822 date or an integer value representing delta(seconds).
 * TODO: We need a way to separate between delta and absolute time.
 */
static status_t parse_retry_after(http_response r, const char *value, error e)
{
    time_t t;
    long delta;

    assert(r != NULL);
    assert(value != NULL);

    if ((t = parse_rfc822_date(value)) != (time_t)-1)
        response_set_retry_after(r, t);
    else if ((delta = atol(value)) <= 0)
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    else
        response_set_retry_after(r, delta);

    return success;
}

static status_t parse_vary(http_response r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!response_set_vary(r, value))
        return set_os_error(e, errno);

    return success;
}

static const struct {
    const char *name;
    status_t (*handler)(http_response req, const char *value, error e);
} response_header_fields[] = {
    { "accept-ranges",      parse_accept_ranges },
    { "age",                parse_age },
    { "etag",               parse_etag },
    { "location",           parse_location },
    { "proxy-authenticate", parse_proxy_authenticate },
    { "retry-after",        parse_retry_after },
    { "server",             parse_server },
    { "vary",               parse_vary },
    { "www-authenticate",   parse_www_authenticate }
};

int find_response_header(const char *name)
{
    int i, n = sizeof response_header_fields / sizeof *response_header_fields;
    for (i = 0; i < n; i++) {
        if (strcmp(response_header_fields[i].name, name) == 0)
            return i;
    }

    return -1;
}

status_t parse_response_header(int idx, http_response req, const char *value, error e)
{
    assert(idx >= 0);
    assert((size_t)idx < sizeof response_header_fields / sizeof *response_header_fields);

    return response_header_fields[idx].handler(req, value, e);
}

/* The response status line(§6.1) is
 *		HTTP-Version SP Status-Code SP Reason-Phrase CRLF
 * It is the first line in all HTTP responses.
 */
static status_t read_response_status_line(http_response response, connection conn, error e)
{
    char *s, buf[CCH_STATUSLINE_MAX + 1];
    int status_code;
    enum http_version version;

    if (!read_line(conn, buf, sizeof buf - 1, e))
        return failure;

    /* The string must start with either HTTP/1.0 or HTTP/1.1 SP */
    if (strstr(buf, "HTTP/1.0 ") == buf)
        version = VERSION_10;
    else if (strstr(buf, "HTTP/1.1 ") == buf)
        version = VERSION_11;
    else
        return set_http_error(e, HTTP_400_BAD_REQUEST);

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
    return success;
}

/* Reads all (if any) http header fields */
static status_t
read_response_header_fields(connection conn, http_response response, error e)
{

    for (;;) {
        char buf[CCH_FIELDNAME_MAX + CCH_FIELDVALUE_MAX + 10];
        char name[CCH_FIELDNAME_MAX + 1];
        char value[CCH_FIELDVALUE_MAX + 1];

        if (!read_line(conn, buf, sizeof buf, e))
            return failure;

        // An empty buffer means that we have read the \r\n sequence
        // separating header fields from entities or terminating the
        // message. This means that there are no more header fields to read.
        if (strlen(buf) == 0)
            return success;

        if (!get_field_name(buf, name, sizeof name)
        || !get_field_value(buf, value, sizeof value))
            return set_http_error(e, HTTP_400_BAD_REQUEST);

        fs_lower(name);
        if (!parse_response_headerfield(name, value, response, e))
            return failure;
    }
}

static bool allws(char *s)
{
    while (isspace(*s))
        s++;

    return *s == '\0';
}

static status_t get_chunklen(connection conn, size_t *len)
{
    char buf[1024];

    if (!connection_gets(conn, buf, sizeof buf))
        return failure;

    if (allws(buf) && !connection_gets(conn, buf, sizeof buf))
        return failure;

    ltrim(buf);
    rtrim(buf);

    return hextosize_t(buf, len);
}


/* Chunked responses start with a chunk length on a separate line,
 * then the chunk follows. the last chunk length will be 0, indicating
 * end of chunk.
 *
 * We may have to reallocate a bit here and there, since we don't know
 * the total size up front.
 */
static status_t read_chunked_response(http_response this, connection conn,
    size_t max_contentlen, error e)
{
    size_t chunklen, ntotal = 0;
    ssize_t nread;
    char *content = NULL, *tmp;

    for (;;) {
        if (!get_chunklen(conn, &chunklen))
            return failure;

        if (chunklen == 0)
            break;

        if (ntotal + chunklen > max_contentlen) {
            free(content);
            return set_app_error(e, ENOSPC);
        }

        // Make sure we have memory to read into
        tmp = realloc(content, ntotal + chunklen);
        if (tmp == NULL) {
            free(content);
            return set_os_error(e, ENOSPC);
        }

        content = tmp;

        // Now read the chunk off the socket.
        // Big chunks may cause timeout, so we loop
        for (;;) {
            nread = connection_read(conn, content + ntotal, chunklen);
            if (nread < 0) {
                free(content);
                return failure;
            }

            ntotal += nread;
            chunklen -= nread;
            if (chunklen == 0)
                break;
        }
    }

    if (content == NULL) {
        return failure;
    }
    response_set_allocated_content_buffer(this, content, ntotal);
    return success;
}

status_t response_receive(http_response response, connection conn,
    size_t max_contentlen, error e)
{
    general_header gh = response_get_general_header(response);
    entity_header eh = response_get_entity_header(response);
    size_t contentlen;
    char *content;
    ssize_t nread;

    if (!read_response_status_line(response, conn, e))
        return failure;

    /* Now read and parse all fields (if any) */
    if (!read_response_header_fields(conn, response, e))
        return failure;

    // 20251015: Cloudflare(CF) violates the RFCs and send TE for non-200,
    // like 301 Moved Permanently. And of course they throw in non-standard
    // header fields like CF-RAY and X-content-type-options. Fuck'em.
    // Do an early successful exit for non-200 non-errors.
    int status = response_get_status(response);
    switch (status) {
        case 204: // OK, but no content
        case 301: // Permanently moved
        case 302: // Temp. moved
        case 304: // Not Modified
            return success;

    }

    /* Now we hopefully have a content-length field. See if we can read
     * it or if it is too big. */
    if (entity_header_content_length_isset(eh)) {
        contentlen = entity_header_get_content_length(eh);
        if (contentlen == 0)
            return success;

        if (contentlen > max_contentlen)
            return set_app_error(e, ENOSPC);
    }
    else if (general_header_is_chunked_message(gh)) {
        return read_chunked_response(response, conn, max_contentlen, e);
    }
    else {
        /* No content length, then we MUST deal with a version 1.0 server.
         * Read until max_contentlen is reached or socket is closed.  */
        contentlen = max_contentlen;
    }

    if ((content = malloc(contentlen + 1)) == NULL)
        return set_os_error(e, errno);

    // We assume that content is string, so add a string terminator
    // for convenience.
    content[contentlen] = '\0';

    if ((nread = connection_read(conn, content, contentlen)) == -1) {
        free(content);
        return set_os_error(e, errno);
    }

    response_set_allocated_content_buffer(response, content, nread);
    return success;
}

void response_dump(http_response r, void *file)
{
    FILE *f = file;

    /* Now our own fields */
    const char *version = "Unknown";
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
        if (!cookie_dump(c, f))
            continue;
    }
}
