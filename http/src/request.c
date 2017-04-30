/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#include <meta_misc.h>
#include <meta_pair.h>
#include <meta_list.h>

#include "highlander.h"
#include "internals.h"
#include "rfc1738.h"

typedef unsigned long flagtype;
static int request_flag_is_set(http_request r, flagtype flag);
static void request_set_flag(http_request r, flagtype flag);
static void request_clear_flags(http_request request);
#define REQUEST_ENTITY_SET					(0x001)
#define REQUEST_URI_SET						(0x002)
#define REQUEST_ACCEPT_SET					(0x004)
#define REQUEST_ACCEPT_CHARSET_SET			(0x008)
#define REQUEST_ACCEPT_ENCODING_SET			(0x010)
#define REQUEST_ACCEPT_LANGUAGE_SET			(0x020)
#define REQUEST_AUTHORIZATION_SET			(0x040)
#define REQUEST_FROM_SET					(0x080)
#define REQUEST_PRAGMA_SET					(0x100)
#define REQUEST_REFERER_SET					(0x200)
#define REQUEST_USER_AGENT_SET				(0x400)
#define REQUEST_LINK_SET					(0x800)
#define REQUEST_MAX_FORWARDS_SET			(0x1000)
#define REQUEST_MIME_VERSION_SET			(0x2000)
#define REQUEST_PROXY_AUTHORIZATION_SET		(0x4000)
#define REQUEST_RANGE_SET					(0x8000)
#define REQUEST_TE_SET						(0x10000)
#define REQUEST_TITLE_SET					(0x20000)
#define REQUEST_UPGRADE_SET					(0x40000)
#define REQUEST_EXPECT_SET					(0x80000)
#define REQUEST_HOST_SET					(0x100000)
#define REQUEST_IF_MATCH_SET				(0x200000)
#define REQUEST_IF_NONE_MATCH_SET			(0x400000)
#define REQUEST_IF_RANGE_SET				(0x800000)
#define REQUEST_IF_MODIFIED_SINCE_SET		(0x1000000)
#define REQUEST_IF_UNMODIFIED_SINCE_SET		(0x2000000)

struct http_request_tag {
    enum http_method method;
    enum http_version version;

    /* We allow others to access our connection, but DO NOT use it ourselves */
    connection external_conn;

    /* Set to true if we want do delay the read of posted content,
     * and to false if we want to read it automagically. Default is false
     */
    int defered_read;

    general_header general_header;
    entity_header entity_header;

    /* Here are some flags to check whether a parameter is set or not.
     * Access them using
     *	request_flag_is_set(http_request, FLAG)
     *	request_set_flag(http_request, FLAG)
     *	request_clear_flags(http_request)
     * to minimize the bug sources :-)
     */
    flagtype flags;

    /* The uri requested and parameters (if any) */
    cstring uri;

    /* NOTE: (2004-07-24) The pair adt can probably be replaced with meta_map*/
    pair params;
    list cookies;

    cstring accept;				/* v1.0 ?D.2.1 v1.1 §14.1 */
    cstring accept_charset;		/* v1.0 §D.2.2 v1.1 §14.2 */
    cstring accept_encoding;	/* v1.0 §D.2.3 v1.1 §14.3 */
    cstring accept_language;	/* v1.0 §D.2.4 v1.1 §14.4 */
    cstring authorization;		/* v1.0 §10.2 v1.1 §14.8 */
    cstring expect;				/* v1.1 §14.20 */
    cstring from;				/* v1.0 §10.8 v1.1 §14.22 */
    cstring host;				/* v1.1 §14.23 */
    cstring if_match;			/* v1.1 §14.24 */
    time_t if_modified_since;	/* v1.0 §10.9 v1.1 §14.25 */
    cstring if_none_match;		/* v1.1 §14.26 */
    cstring if_range;			/* v1.1 §14.27 */
    time_t if_unmodified_since;	/* v1.1 §14.28 */
    int max_forwards;			/* v1.1 §14.31 */
    cstring proxy_authorization;/* v1.1 §14.34 */
    cstring range;				/* v1.1 §14.35 */
    cstring referer;			/* v1.0 §10.13 v1.1 §14.36 */
    cstring te;					/* v1.1 §14.39 */
    cstring user_agent;			/* v1.0 §10.15 v1.1 §14.43 */

    /* Version 1.0 fields */
    cstring link;				/* v1.0 §D.2.6 */
    int mime_version_major;		/* v1.0 §D.2.7 */
    int mime_version_minor;		/* v1.0 §D.2.7 */
    cstring title;				/* v1.0 §D.2.9 */

    /* We may receive data (POST)
     * As it may be encoded in all sort of ways, we
     * keep it as a byte buffer instead of a cstring
     * We alloc and free this buffer.
     */
    char *entity_buf;
};


void request_free(http_request p)
{
    if (p == NULL)
        return;

    if (p->params != NULL) {
        pair_free(p->params);
        p->params = NULL;
    }

    if (p->cookies != NULL) {
        list_free(p->cookies, (dtor)cookie_free);
        p->cookies = NULL;
    }

    if (p->entity_buf != NULL)
        free(p->entity_buf);

    general_header_free(p->general_header);
    entity_header_free(p->entity_header);
    cstring_free(p->uri);
    cstring_free(p->accept);
    cstring_free(p->accept_charset);
    cstring_free(p->accept_encoding);
    cstring_free(p->accept_language);
    cstring_free(p->authorization);
    cstring_free(p->from);
    cstring_free(p->referer);
    cstring_free(p->user_agent);
    cstring_free(p->link);
    cstring_free(p->range);
    cstring_free(p->te);
    cstring_free(p->title);
    cstring_free(p->expect);
    cstring_free(p->host);
    cstring_free(p->if_match);
    cstring_free(p->if_none_match);
    cstring_free(p->if_range);

    free(p);
}

void request_recycle(http_request p)
{
    assert(p != NULL);

    if (p->params != NULL) {
        pair_free(p->params);
        p->params = NULL;
    }

    if (p->cookies != NULL) {
        list_free(p->cookies, (dtor)cookie_free);
        p->cookies = NULL;
    }

    p->external_conn = NULL;

    general_header_recycle(p->general_header);
    entity_header_recycle(p->entity_header);
    request_clear_flags(p);

    /* These columns are multivalued and must be set
     * using cstring_concat(), and read using some other method. */
    cstring_recycle(p->accept);
    cstring_recycle(p->accept_charset);
    cstring_recycle(p->accept_encoding);
    cstring_recycle(p->accept_language);
    cstring_recycle(p->te);

    if (p->entity_buf != NULL) {
        free(p->entity_buf);
        p->entity_buf = NULL;
    }
}

void request_set_method(http_request p, http_method method)
{
    assert(p != NULL);
    p->method = method;
}

status_t request_set_uri(http_request p, const char *value)
{
    assert(p != NULL);
    assert(value != NULL);
    assert(strchr(value, '?') == NULL); /* Params must have been removed */

    if (!cstring_set(p->uri, value))
        return failure;

    request_set_flag(p, REQUEST_URI_SET);
    return success;
}

void request_set_version(http_request p, http_version version)
{
    assert(p != NULL);
    p->version = version;
}

const char* request_get_uri(http_request p)
{
    if (!request_flag_is_set(p, REQUEST_URI_SET))
        return NULL;
    else
        return c_str(p->uri);
}

const char* request_get_referer(http_request p)
{
    if (!request_flag_is_set(p, REQUEST_REFERER_SET))
        return "";
    else
        return c_str(p->referer);
}

http_method request_get_method(http_request p)
{
    assert(p != NULL);
    return p->method;
}

http_version request_get_version(http_request p)
{
    assert(p != NULL);
    return p->version;
}

status_t request_add_param(http_request p, const char *name, const char *value)
{
    assert(p != NULL);
    assert(name != NULL);
    assert(strlen(name) > 0);
    assert(value != NULL);

    if (NULL == p->params) {
        if ((p->params = pair_new(20)) == NULL)
            return failure;
    }

    return pair_set(p->params, name, value);
}

int request_get_parameter_count(const http_request p)
{
    int param_count = 0;
    if (p->params)
        param_count = (int)pair_size(p->params);

    return param_count;
}

const char* request_get_host(http_request p)
{
    assert(p != NULL);

    if (request_flag_is_set(p, REQUEST_HOST_SET))
        return c_str(p->host);
    else
        return NULL;
}

const char* request_get_parameter_name(http_request p, size_t i)
{
    assert(p != NULL);
    return pair_get_name(p->params, i);
}

/*
 * Returns NULL if no params set, else the value of the parameter
 * associated with the name. NULL if name not found in p->params.
 */
const char* request_get_parameter_value(const http_request p, const char *pname)
{
    assert(p != NULL);

    if (p->params == NULL)
        return NULL;

    return pair_get(p->params, pname);
}

status_t request_add_cookie(http_request p, cookie c)
{
    assert(p != NULL);

    if (NULL == p->cookies) {
        if ((p->cookies = list_new()) == NULL)
            return failure;
    }

    return list_add(p->cookies, c) == NULL ? failure : success;
}


size_t request_get_cookie_count(const http_request p)
{
    assert(p != NULL);

    if (NULL == p->cookies)
        return 0;

    return list_size(p->cookies);
}

cookie request_get_cookie(const http_request p, size_t i)
{
    assert(p != NULL);

    return list_get_item(p->cookies, i);
}

status_t request_set_accept(http_request r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_concat(r->accept, value))
        return set_os_error(e, errno);

    request_set_flag(r, REQUEST_ACCEPT_SET);
    return success;
}

status_t request_set_accept_charset(http_request r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_concat(r->accept_charset, value))
        return set_os_error(e, errno);

    request_set_flag(r, REQUEST_ACCEPT_CHARSET_SET);
    return success;
}

status_t request_set_accept_encoding(http_request r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_concat(r->accept_encoding, value))
        return set_os_error(e, errno);

    request_set_flag(r, REQUEST_ACCEPT_ENCODING_SET);
    return success;
}

status_t request_set_accept_language(http_request r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_concat(r->accept_language, value))
        return set_os_error(e, errno);

    request_set_flag(r, REQUEST_ACCEPT_LANGUAGE_SET);
    return success;
}

status_t request_set_authorization(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->authorization, value))
        return failure;

    request_set_flag(r, REQUEST_AUTHORIZATION_SET);
    return success;
}

status_t request_set_from(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->from, value))
        return failure;

    request_set_flag(r, REQUEST_FROM_SET);
    return success;
}

void request_set_if_modified_since(http_request r, time_t value)
{
    assert(r != NULL);
    assert(value != (time_t)-1);

    r->if_modified_since = value;
    request_set_flag(r, REQUEST_IF_MODIFIED_SINCE_SET);
}

status_t request_set_referer(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->referer, value))
        return failure;

    request_set_flag(r, REQUEST_REFERER_SET);
    return success;
}

status_t request_set_user_agent(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->user_agent, value))
        return failure;

    request_set_flag(r, REQUEST_USER_AGENT_SET);
    return success;
}

#if 0
Tue Feb 12 18:20:04 CET 2002
Not in use?

int request_set_link(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->link, value))
        return set_os_error(e, ENOMEM);

    request_set_flag(r, REQUEST_LINK_SET);
    return success;
}

int request_set_title(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->title, value))
        return set_os_error(e, ENOMEM);

    request_set_flag(r, REQUEST_TITLE_SET);
    return success;
}

#endif


status_t request_set_mime_version(http_request r, int major, int minor, error e)
{
    assert(r != NULL);
    assert(major);

    /* We only understand MIME 1.0 */
    if (major != 1 || minor != 0)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    r->mime_version_major = major;
    r->mime_version_minor = minor;
    request_set_flag(r, REQUEST_MIME_VERSION_SET);
    return success;
}

status_t request_set_range(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->range, value))
        return failure;

    request_set_flag(r, REQUEST_RANGE_SET);
    return success;
}

status_t request_set_te(http_request r, const char *value, error e)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_concat(r->te, value))
        return set_os_error(e, errno);

    request_set_flag(r, REQUEST_TE_SET);
    return success;
}

status_t request_set_expect(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->expect, value))
        return failure;

    request_set_flag(r, REQUEST_EXPECT_SET);
    return success;
}

status_t request_set_host(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);
    assert(!request_flag_is_set(r, REQUEST_HOST_SET));

    if (!cstring_set(r->host, value))
        return failure;

    request_set_flag(r, REQUEST_HOST_SET);
    return success;
}

status_t request_set_if_match(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->if_match, value))
        return failure;

    request_set_flag(r, REQUEST_IF_MATCH_SET);
    return success;
}

status_t request_set_if_none_match(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->if_none_match, value))
        return failure;

    request_set_flag(r, REQUEST_IF_NONE_MATCH_SET);
    return success;
}

status_t request_set_if_range(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->if_range, value))
        return failure;

    request_set_flag(r, REQUEST_IF_RANGE_SET);
    return success;
}

status_t request_set_if_unmodified_since(http_request r, time_t value)
{
    assert(r != NULL);
    assert(value != (time_t)-1);

    r->if_unmodified_since = value;
    request_set_flag(r, REQUEST_IF_UNMODIFIED_SINCE_SET);
    return success;
}

void request_set_max_forwards(http_request r, unsigned long value)
{
    r->max_forwards = value;
    request_set_flag(r, REQUEST_MAX_FORWARDS_SET);
}

static int request_flag_is_set(http_request r, flagtype flag)
{
    assert(r != NULL);
    assert(flag > 0);

    return r->flags & flag ? 1 : 0;
}

static void request_set_flag(http_request r, flagtype flag)
{
    assert(r != NULL);
    assert(flag > 0);

    r->flags |= flag;
}

static void request_clear_flags(http_request r)
{
    assert(r != NULL);
    r->flags = 0;
}

http_request request_new(void)
{
    http_request p;
    cstring arr[18];

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    request_clear_flags(p);

    if ((p->general_header = general_header_new()) == NULL)
        goto err;

    if ((p->entity_header = entity_header_new()) == NULL)
        goto err;

    if (!cstring_multinew(arr, sizeof arr / sizeof *arr)) 
        goto err;

    p->version = VERSION_UNKNOWN;
    p->method = METHOD_UNKNOWN;
    p->entity_buf = NULL;

    p->uri = arr[0];
    p->accept = arr[1];
    p->accept_charset = arr[2];
    p->accept_encoding = arr[3];
    p->accept_language = arr[4];
    p->authorization = arr[5];
    p->from = arr[6];
    p->referer = arr[7];
    p->user_agent = arr[8];
    p->link = arr[9];
    p->range = arr[10];
    p->te = arr[11];
    p->title = arr[12];
    p->expect = arr[13];
    p->host = arr[14];
    p->if_match = arr[15];
    p->if_none_match = arr[16];
    p->if_range = arr[17];

    return p;

err:
    general_header_free(p->general_header);
    entity_header_free(p->entity_header);
    request_free(p);
    return NULL;
}

bool request_accepts_media_type(http_request r, const char *val)
{
    /* We accept if request has no opinion */
    if (!request_flag_is_set(r, REQUEST_ACCEPT_SET))
        return true;

    /* The client accepts a media type if we find it */
    return strstr(c_str(r->accept), val) == NULL ? false : true;
}

bool request_accepts_language(http_request r, const char *val)
{
    /* We do not want to mix e.g. "en" and "den" so check every one */
    char buf[CCH_LANGUAGE_MAX + 1];
    const char *s = c_str(r->accept_language);
    size_t i = 0;

    /* We accept the language if request has no opinion */
    if (!request_flag_is_set(r, REQUEST_ACCEPT_LANGUAGE_SET))
        return true;

    for (i = 0;; i++) {
        if (!get_word_from_string(s, buf, sizeof buf, i))
            return false; /* No more words in string. */

        if (0 == strcmp(buf, val))
            return true;
    }
}

status_t request_set_proxy_authorization(http_request r, const char *value)
{
    assert(r != NULL);
    assert(value != NULL);

    if (!cstring_set(r->proxy_authorization, value))
        return failure;

    request_set_flag(r, REQUEST_PROXY_AUTHORIZATION_SET);
    return success;
}

const char* request_get_content(http_request p)
{
    return p->entity_buf;
}

status_t request_set_entity(http_request p, void* entity, size_t cb)
{
    assert(entity_header_content_length_isset(p->entity_header));
    assert(cb == entity_header_get_content_length(p->entity_header));
    assert(cb > 0);
    assert(p->entity_buf == NULL);

    if ((p->entity_buf = malloc(cb)) == NULL)
        return failure;

    memcpy(p->entity_buf, entity, cb);
    return success;
}

const char* request_get_user_agent(http_request p)
{
    assert(p != NULL);
    if (request_flag_is_set(p, REQUEST_USER_AGENT_SET))
        return c_str(p->user_agent);
    else
        return "";
}

/* Fields are separated with &, and there is no leading ?.
 * One & means 2 fields.
 */
size_t request_get_field_count(http_request request)
{
    size_t i, nelem = 0, cb;

    assert(request != NULL);
    assert(entity_header_content_length_isset(request->entity_header));
    assert(request->entity_buf != NULL);

    cb = request_get_content_length(request);
    if (cb > 0)
        nelem = 1;

    for (i = 0; i < cb; i++) {
        if (request->entity_buf[i] == '&')
            nelem++;
    }

    return nelem;
}

/* Return a pointer to the start of field n, where & is field separator */
static const char* get_field_start(const char *content, size_t cb, size_t idx)
{
    const char *start, *stop;

    start = content;
    stop = content + cb;

    while (idx > 0 && start < stop) {
        if (*start == '&')
            idx--;

        start++;
    }

    return start;
}

size_t request_get_field_namelen(http_request request, size_t idx)
{
    const char *start, *end;
    size_t cb, fields;

    assert(request != NULL);
    assert(entity_header_content_length_isset(request->entity_header));
    assert(request->entity_buf != NULL);

    fields = request_get_field_count(request);
    assert(idx < fields);

    /* Do not allow overflow */
    if (idx >= fields)
        return 0;

    cb = request_get_content_length(request);
    start = get_field_start(request->entity_buf, cb, idx);
    end = request->entity_buf + cb;

    cb = 0;
    while (start < end && *start != '=') {
        cb++;
        start++;
    }

    if (start == end)
        return 0;

    return cb;
}

size_t request_get_field_valuelen(http_request request, size_t idx)
{
    const char *start, *stop;
    size_t cb, bufsiz;

    assert(request != NULL);
    assert(entity_header_content_length_isset(request->entity_header));

    bufsiz = request_get_content_length(request);
    start = get_field_start(request->entity_buf, bufsiz, idx);
    cb = request_get_field_namelen(request, idx);
    if (cb == 0)
        return 0;

    start += cb;

    /* skip the '=' */
    assert(*start == '=');
    start++;
    cb = 0;

    stop = request->entity_buf + bufsiz;
    while (start < stop && *start != '&') {
        cb++;
        start++;
    }

    return cb;
}

status_t request_get_field_name(http_request request, size_t i, char *dest, size_t destsize)
{
    const char *start;
    size_t fieldlen;

    assert(request != NULL);
    assert(entity_header_content_length_isset(request->entity_header));

    start = get_field_start(request->entity_buf, request_get_content_length(request), i);

    if (start == NULL)
        return failure;

    if ((fieldlen = request_get_field_namelen(request, i)) == 0)
        return failure;

    if (destsize < fieldlen)
        fieldlen = destsize;

    while (fieldlen--)
        *dest++ = *start++;

    *dest = '\0';
    return success;
}

status_t request_get_field_value(http_request request, size_t i, char *dest, size_t destsize)
{
    const char *start;
    size_t namelen, valuelen;

    assert(request != NULL);
    assert(entity_header_content_length_isset(request->entity_header));

    start = get_field_start(request->entity_buf, request_get_content_length(request), i);
    namelen = request_get_field_namelen(request, i);
    valuelen = request_get_field_valuelen(request, i);

    if (namelen == 0 || valuelen == 0)
        return failure;

    start += namelen;
    assert(*start == '=');
    start++; /* Skip '=' */

    rfc1738_decode(dest, destsize, start, valuelen);

    /* So far so good. We have a copy. Replace the + with space */
    while (*dest != '\0') {
        if (*dest == '+')
            *dest = ' ';

        dest++;
    }

    return success;
}

/* Just iterate on fieldcount and look for the correct name. Then
 * copy the value and return.
 */
status_t request_get_field_value_by_name(http_request request, const char *name, char *value, size_t cb)
{
    size_t i, fieldcount;

    assert(request != NULL);
    assert(name != NULL);
    assert(value != NULL);
    assert(cb != 0);

    if ((fieldcount = request_get_field_count(request)) == 0)
        return failure;

    for (i = 0; i < fieldcount; i++) {
        char sz[10240];

        if (!request_get_field_name(request, i, sz, sizeof(sz) - 1))
            return failure;

        if (strcmp(sz, name) == 0)
            return request_get_field_value(request, i, value, cb);
    }

    return failure; /* Name not found */
}

time_t request_get_if_modified_since(http_request r)
{
    assert(r != NULL);

    if (request_flag_is_set(r, REQUEST_IF_MODIFIED_SINCE_SET))
        return r->if_modified_since;

    return (time_t)-1;
}

general_header request_get_general_header(http_request r)
{
    assert(r != NULL);
    return r->general_header;
}

entity_header request_get_entity_header(http_request r)
{
    assert(r != NULL);
    return r->entity_header;
}

size_t request_get_content_length(http_request request)
{
    assert(request != NULL);
    if (!entity_header_content_length_isset(request->entity_header))
        return 0; /* This is actually an error */

    return entity_header_get_content_length(request->entity_header);
}

/* Helper function to have the algorithm one place only */
static status_t req_parse_multivalued_fields(
    http_request req,
    const char* value,
    status_t(*set_func)(http_request req, const char* value, error e),
    error e)
{
    const int sep = ',';
    char buf[100];
    char* s;

    assert(req != NULL);
    assert(value != NULL);

    while ((s = strchr(value, sep)) != NULL) {
        /* The correct type would be ptrdiff_t,
         * but -ansi -pedantic complains */
        size_t span = (size_t)(s - value);
        if (span + 1 > sizeof buf) {
            /* We don't want buffer overflow... */
            value = s + 1;
            continue;
        }

        memcpy(buf, value, span);
        buf[span] = '\0';
        if (!set_func(req, buf, e))
            return 0;

        value = s + 1;
    }

    return set_func(req, value, e);
}
/* http request handlers */
static status_t parse_authorization(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_authorization(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_expect(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_expect(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_if_match(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_if_match(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_if_modified_since(http_request req, const char* value, error e)
{
    time_t d;

    assert(req != NULL);
    assert(value != NULL);

    /* Parse date and create a time_t */
    if ((d = parse_rfc822_date(value)) == (time_t)-1)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    request_set_if_modified_since(req, d);
    return success;
}

static status_t parse_if_none_match(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_if_none_match(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_if_range(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_if_range(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_max_forwards(http_request req, const char* value, error e)
{
    unsigned long v;

    assert(req != NULL);
    assert(value != NULL);

    if ((v = strtoul(value, NULL, 10)) == 0)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    request_set_max_forwards(req, v);
    return success;
}

static status_t parse_proxy_authorization(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_proxy_authorization(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_if_unmodified_since(http_request req, const char* value, error e)
{
    time_t d;

    assert(req != NULL);
    assert(value != NULL);

    /* Parse date and create a time_t */
    if ((d = parse_rfc822_date(value)) == (time_t)-1)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    return request_set_if_unmodified_since(req, d);
}

static status_t parse_range(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_range(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_referer(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);

    if (!request_set_referer(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_te(http_request req, const char* value, error e)
{
    assert(req != NULL);
    assert(value != NULL);
    return req_parse_multivalued_fields(req, value, request_set_te, e);
}

static status_t parse_mime_version(http_request r, const char* value, error e)
{
    /* See rfc 2045 for syntax (MIME-Version=x.y) */
    int major, minor;
    assert(r != NULL);
    assert(value != NULL);

    major = 0;
    while (*value != '\0' && *value != '.') {
        major = major * 10 + (int)(*value - '0');
        value++;
    }

    if ('\0' == *value)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    /* Skip . */
    value++;
    minor = 0;
    while (*value) {
        minor = minor * 10 + (int)(*value - '0');
        value++;
    }

    /* NOTE: The code above allows for "1." which is incorrect.
     * We will not fail, though
     */
    return request_set_mime_version(r, major, minor, e);
}

static status_t parse_from(http_request req, const char* value, error e)
{
    if (!request_set_from(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_host(http_request req, const char* value, error e)
{
    if (!request_set_host(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_user_agent(http_request req, const char* value, error e)
{
    if (!request_set_user_agent(req, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_accept(http_request req, const char* value, error e)
{
    return req_parse_multivalued_fields(req, value, request_set_accept, e);
}

static status_t parse_accept_charset(http_request req, const char* value, error e)
{
    return req_parse_multivalued_fields(req, value, request_set_accept_charset, e);
}

static status_t parse_accept_encoding(http_request req, const char* value, error e)
{
    return req_parse_multivalued_fields(req, value, request_set_accept_encoding, e);
}

static status_t parse_accept_language(http_request req, const char* value, error e)
{
    return req_parse_multivalued_fields(req, value, request_set_accept_language, e);
}

/* The request line, defined in §5.1, is
 *		 Method SP Request-URI SP HTTP-Version CRLF
 */
static status_t send_request_line(http_request r, connection c, error e)
{
    cstring s;
    const char *p;

    if ((s = cstring_new()) == NULL)
        return set_os_error(e, errno);

    switch (request_get_method(r)) {
        case METHOD_HEAD:
            if (!cstring_concat(s, "HEAD "))
                goto err;

            break;

        case METHOD_GET:
            if (!cstring_concat(s, "GET "))
                goto err;

            break;

        case METHOD_POST:
            if (!cstring_concat(s, "POST "))
                goto err;

            break;

        default:
            cstring_free(s);
            return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    if ((p = request_get_uri(r)) == NULL) {
        cstring_free(s);
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    if (!cstring_concat(s, p)) {
        cstring_free(s);
        return set_os_error(e, errno);
    }

    switch (request_get_version(r)) {
        case VERSION_10:
            if (!cstring_concat(s, " HTTP/1.0\r\n"))
                goto err;

            break;

        case VERSION_11:
            if (!cstring_concat(s, " HTTP/1.1\r\n"))
                goto err;

            break;

        default:
            cstring_free(s);
            return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    /* Now send it */
    if (!connection_write(c, c_str(s), cstring_length(s))) {
        cstring_free(s);
        return set_os_error(e, errno);
    }

    cstring_free(s);
    return success;

err:
    cstring_free(s);
    return set_http_error(e, HTTP_503_SERVICE_UNAVAILABLE);
}

static status_t send_accept(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Accept: ", r->accept);
}

static status_t send_accept_charset(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Accept-Charset: ", r->accept_charset);
}

static status_t send_accept_encoding(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Accept-Encoding: ", r->accept_encoding);
}

static status_t send_accept_language(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Accept-Language: ", r->accept_language);
}

static status_t send_authorization(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Authorization: ", r->authorization);
}

static status_t send_from(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "From: ", r->from);
}

static status_t send_referer(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Referer: ", r->referer);
}

static status_t send_user_agent(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "User-Agent: ", r->user_agent);
}

static status_t send_max_forwards(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_ulong(c, "Max-Forwards: ", r->max_forwards);
}

static status_t send_proxy_authorization(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Proxy-Authorization: ", r->proxy_authorization);
}

static status_t send_range(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Range: ", r->range);
}

static status_t send_te(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "TE: ", r->te);
}

static status_t send_expect(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Expect: ", r->expect);
}

static status_t send_host(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "Host: ", r->host);
}

static status_t send_if_match(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "If-Match: ", r->if_match);
}

static status_t send_if_none_match(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "If-None-Match: ", r->if_none_match);
}

static status_t send_if_range(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_field(c, "If-Range: ", r->if_range);
}

static status_t send_if_modified_since(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_date(c, "If-Modified-Since: ", r->if_modified_since);
}

static status_t send_if_unmodified_since(http_request r, connection c)
{
    assert(r != NULL);
    assert(c != NULL);

    return http_send_date(c, "If-Unmodified-Since: ", r->if_unmodified_since);
}

static status_t request_send_fields(http_request r, connection c)
{
    static const struct {
        flagtype flag;
        status_t(*func)(http_request, connection);
    } fields[] = {
        { REQUEST_ACCEPT_SET,				send_accept },
        { REQUEST_ACCEPT_CHARSET_SET,		send_accept_charset },
        { REQUEST_ACCEPT_ENCODING_SET,		send_accept_encoding },
        { REQUEST_ACCEPT_LANGUAGE_SET,		send_accept_language },
        { REQUEST_AUTHORIZATION_SET,		send_authorization },
        { REQUEST_EXPECT_SET,				send_expect },
        { REQUEST_FROM_SET,					send_from },
        { REQUEST_HOST_SET,					send_host },
        { REQUEST_IF_MATCH_SET,				send_if_match },
        { REQUEST_IF_NONE_MATCH_SET,		send_if_none_match },
        { REQUEST_IF_RANGE_SET,				send_if_range },
        { REQUEST_IF_MODIFIED_SINCE_SET,	send_if_modified_since },
        { REQUEST_IF_UNMODIFIED_SINCE_SET,	send_if_unmodified_since },
        { REQUEST_MAX_FORWARDS_SET,			send_max_forwards },
        { REQUEST_PROXY_AUTHORIZATION_SET,	send_proxy_authorization },
        { REQUEST_RANGE_SET,				send_range },
        { REQUEST_REFERER_SET,				send_referer },
        { REQUEST_TE_SET,					send_te },
        { REQUEST_USER_AGENT_SET,			send_user_agent },
        #if 0
        /* Version 1.0 */
        { REQUEST_LINK_SET,					send_link },
        { REQUEST_MIME_VERSION_SET,			send_mime_version },
        { REQUEST_TITLE_SET,				send_title },
        { REQUEST_UPGRADE_SET,				send_upgrade },
        { REQUEST_PRAGMA_SET,				send_pragma },
        #endif
    };

    size_t i, nelem = sizeof fields / sizeof *fields;

    for (i = 0; i < nelem; i++) {
        if (request_flag_is_set(r, fields[i].flag))
            if (!fields[i].func(r, c))
                return failure;
    }

    return success;
}


status_t request_send(http_request r, connection c, error e)
{
    assert(r != NULL);
    assert(c != NULL);
    (void)e; /* for now, we may want to add semantic checks later */
 
    if (!send_request_line(r, c, e))
        return failure;

    if (!general_header_send_fields(r->general_header, c))
        return failure;

    if (!entity_header_send_fields(r->entity_header, c))
        return failure;

    /* Now send request fields */
    if (!request_send_fields(r, c))
        return failure;

    /* Now terminate the request */
    if (!connection_write(c, "\r\n", 2))
        return failure;

    if (!connection_flush(c))
        return failure;

    return success;
}

static status_t read_posted_content(
    size_t max_post_content,
    connection conn,
    http_request req,
    error e)
{
    void *buf;
    size_t content_len;
    ssize_t nread;

    content_len = request_get_content_length(req);
    if (content_len == 0)
        return set_http_error(e, HTTP_411_LENGTH_REQUIRED);

    if (max_post_content < content_len)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    if ((buf = malloc(content_len)) == NULL)
        return set_os_error(e, errno);

    if ((nread = connection_read(conn, buf, content_len)) == -1) {
        free(buf);
        return set_tcpip_error(e, errno);
    }

    /* Short reads are not acceptable here */
    if ((size_t)nread != content_len) {
        free(buf);
        return set_tcpip_error(e, EINVAL);
    }

    if (!request_set_entity(req, buf, nread)) {
        free(buf);
        return set_os_error(e, ENOSPC);
    }

    /* success */
    free(buf);
    return success;
}

status_t get_field_name(const char* src, char* dest, size_t destsize)
{
    char *s;
    size_t span;

    if ((s = strchr(src, ':')) == NULL)
        return failure;

    span = (size_t)(s - src);
    if (span + 1 >= destsize)
        return failure;

    memcpy(dest, src, span);
    dest[span] = '\0';
    return success;
}

/*
 * See get_field_name() for more info.
 */
status_t get_field_value(const char* src, char* dest, size_t destsize)
{
    /* Locate separator as in name: value */
    const char *s = strchr(src, ':');
    size_t len;

    if (s == NULL)
        return failure;

    /* Skip : and any spaces after separator */
    s++;
    while (isspace((int)*s))
        s++;

    len = strlen(s);
    if (len >= destsize)
        return failure;

    memcpy(dest, s, len + 1);
    return success;
}

/*
 * Input is normally "name: value"
 * as in Host: www.veryfast.com
 */
static status_t parse_one_field(
    connection conn,
    http_request request,
    const char* buf,
    error e)
{
    char name[CCH_FIELDNAME_MAX + 1];
    char value[CCH_FIELDVALUE_MAX + 1];

    if (!get_field_name(buf, name, sizeof name)
    || !get_field_value(buf, value, sizeof value))
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    fs_lower(name);
    return parse_request_headerfield(conn, name, value, request, e);
}


/* Reads all (if any) http header fields */
static status_t
read_request_header_fields(connection conn, http_request request, error e)
{
    for (;;) {
        char buf[CCH_FIELDNAME_MAX + CCH_FIELDVALUE_MAX + 10];

        if (!read_line(conn, buf, sizeof buf, e))
            return failure;

        /*
         * An empty buffer means that we have read the \r\n sequence
         * separating header fields from entities or terminating the
         * message. This means that there is no more header fields to read.
         */
        if (strlen(buf) == 0)
            return success;

        if (!parse_one_field(conn, request, buf, e))
            return failure;
    }
}

static http_method get_method(const char* str)
{
    static const struct {
        const char* str;
        const enum http_method id;
    } methods[] = {
        { "GET", METHOD_GET, },
        { "HEAD",METHOD_HEAD, },
        { "POST", METHOD_POST, }
    };

    size_t i, cMethods = sizeof(methods) / sizeof(methods[0]);
    http_method method = METHOD_UNKNOWN;

    for (i = 0; i < cMethods; i++) {
        if (0 == strcmp(methods[i].str, str)) {
            method = methods[i].id;
            break;
        }
    }

    return method;
}

static http_version get_version(const char* s)
{
    static struct {
        const char* str;
        const enum http_version id;
    } versions[] = {
        { "HTTP/1.0", VERSION_10, },
        { "HTTP/1.1", VERSION_11, }
    };

    http_version version = VERSION_UNKNOWN;
    size_t i, n = sizeof(versions) / sizeof(versions[0]);

    for (i = 0; i < n; i++) {
        if (0 == strcmp(s, versions[i].str)) {
            version = versions[i].id;
            break;
        }
    }

    return version;
}


static status_t
parse_request_method(const char* line, http_request request, error e)
{
    char strMethod[CCH_METHOD_MAX + 1];
    http_method method;

    if (!get_word_from_string(line, strMethod, sizeof strMethod, 0))
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    if ((method = get_method(strMethod)) == METHOD_UNKNOWN)
        return set_http_error(e, HTTP_501_NOT_IMPLEMENTED);

    request_set_method(request, method);
    return success;
}

static inline int more_uri_params_available(const char *s)
{
    assert(s != NULL);
    assert(*s != '\0');

    return strchr(s, '=') != NULL;
}

static status_t get_uri_param_name(const char* src, char dest[], size_t destsize, error e)
{

    /* '=' is required */
    if (strchr(src, '=') == NULL)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    if (!copy_word(src, dest, '=', destsize))
        return set_http_error(e, HTTP_414_REQUEST_URI_TOO_LARGE);

    return success;
}

/* Locate '=' and skip it, then copy value */
static status_t get_uri_param_value(const char* src, char dest[], size_t destsize, error e)
{
    char *p;

    p = strchr(src, '=');
    if (NULL == p)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    if (!copy_word(++p, dest, '&', destsize))
        return set_http_error(e, HTTP_414_REQUEST_URI_TOO_LARGE);

    return success;
}


/* Returns NULL if ;, which separates the args, isn't found */
static char* locate_next_uri_param(char *s)
{
    /* Locate next argument */
    char *p = strchr(s, '&');
    if (p)
        s = p + 1; /* Skip '&' */
    else
        s = NULL;

    return s;
}

static inline status_t
decode_uri_param_value(char* decoded, const char* value, size_t cb, error e)
{
    int rc = rfc1738_decode(decoded, cb, value, strlen(value));
    if (rc == 0) {
        if (errno == EINVAL)
            return set_http_error(e, HTTP_400_BAD_REQUEST);
        else
            return set_os_error(e, errno);
    }

    return success;
}

static status_t set_one_uri_param(http_request request, char *s, error e)
{
    char name[CCH_PARAMNAME_MAX + 1];
    char value[CCH_PARAMVALUE_MAX + 1];
    char decoded[CCH_PARAMVALUE_MAX + 1];

    if (!get_uri_param_name(s, name, sizeof name, e))
        return failure;

    if (!get_uri_param_value(s, value, sizeof value, e))
        return failure;

    if (!decode_uri_param_value(decoded, value, sizeof decoded, e))
        return failure;

    if (!request_add_param(request, name, decoded))
        return set_os_error(e, errno);

    return success;
}

/*
 * Params are name/value pairs separated by ; as in foo=bar;f2=fff
 * We try to support foo=;bar=;foobar=foxx as well
 * We do require the =
 *
 * Will modify contents of s
 * Input is everything after the ? as in "/foo.html?bar=asdad;b2=x"
 */
static status_t set_uri_params(http_request request, char* s, error e)
{
    while (more_uri_params_available(s)) {
        if (!set_one_uri_param(request, s, e))
            return failure;

        if ((s = locate_next_uri_param(s)) == NULL)
            break;
    }

    return success;
}

static inline int uri_has_params(const char* uri)
{
    return strchr(uri, '?') != NULL;
}

static status_t set_uri_and_params(http_request request, char* uri, error e)
{
    char *s;

    assert(uri_has_params(uri));

    /* Look for parameters */
    s = strchr(uri, '?');
    assert(s != NULL);

    /* Cut here to terminate uri */
    *s = '\0';

    if (!request_set_uri(request, uri))
        return set_os_error(e, errno);

    s++; /* Skip the ?, which now is a \0 */

    if (strlen(s) == 0) /* Someone gave us just a URI and a ? */
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    return set_uri_params(request, s, e);
}

static status_t
parse_request_uri(const char* line, http_request request, error e)
{
    char uri[CCH_URI_MAX + 1];

    assert(line != NULL);
    assert(request != NULL);

    if (strlen(line) >= CCH_URI_MAX)
        return set_http_error(e, HTTP_414_REQUEST_URI_TOO_LARGE);

    if (!get_word_from_string(line, uri, sizeof uri, 1))
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    if (uri_has_params(uri))
        return set_uri_and_params(request, uri, e);

    if (!request_set_uri(request, uri))
        return set_os_error(e, errno);

    return success;
}

static status_t
parse_request_version(const char* line, http_request request, error e)
{
    int iword;
    char strVersion[CCH_VERSION_MAX + 1];
    http_version version;

    assert(line != NULL);
    assert(request != NULL);
    assert(e != NULL);

    if ((iword = find_word(line, 2)) == -1) {
        /* No version info == HTTP 0.9 */
        request_set_version(request, VERSION_09);
        return success;
    }

    if (strlen(&line[iword]) > CCH_VERSION_MAX)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    if (!get_word_from_string(line, strVersion, sizeof strVersion, 2))
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    if ((version = get_version(strVersion)) == VERSION_UNKNOWN)
        return set_http_error(e, HTTP_505_HTTP_VERSION_NOT_SUPPORTED);

    request_set_version(request, version);
    return success;
}


/*
 * Returns > 0 if request line wasn't understood, 0 if OK
 * Input is Method SP Request-URI SP [ HTTP-Version ]
 * The CRLF has been removed.
 * See §5.1 for details.
 * We support 0.9, 1.0 and 1.1 and GET, HEAD and POST
 */
static status_t
parse_request_line(const char* line, http_request request, error e)
{
    if (parse_request_method(line, request, e)
    && parse_request_uri(line, request, e)
    && parse_request_version(line, request, e))
        return success;
    else
        return failure;
}

static const struct {
    const char* name;
    status_t (*handler)(http_request req, const char* value, error e);
} request_header_fields[] = {
    { "user-agent",			parse_user_agent },
    { "cookie",				parse_cookie },
    { "host",				parse_host },

    { "from",				parse_from },
    { "accept",				parse_accept },
    { "accept-charset",		parse_accept_charset },
    { "accept-encoding",	parse_accept_encoding },
    { "accept-language",	parse_accept_language },


    { "mime-version",		parse_mime_version },

    /* request-header */
    { "authorization",		parse_authorization },
    { "expect",				parse_expect },
    { "if-match",			parse_if_match },
    { "if-modified-since",	parse_if_modified_since },
    { "if-none-match",		parse_if_none_match },
    { "if-range",			parse_if_range },
    { "if-unmodified-since",parse_if_unmodified_since },
    { "max-forwards",		parse_max_forwards },
    { "proxy-authorization",parse_proxy_authorization },
    { "range",				parse_range },
    { "referer",			parse_referer },
    { "te",					parse_te },
};


/* Return an index in the request header array, or -1 if
 * the field was not found. */
int find_request_header(const char* name)
{
    int i, n = sizeof request_header_fields / sizeof *request_header_fields;
    for (i = 0; i < n; i++) {
        if (strcmp(request_header_fields[i].name, name) == 0)
            return i;
    }

    return -1;
}

status_t parse_request_header(int idx, http_request req, const char* value, error e)
{
    assert(idx >= 0);
    assert((size_t)idx < sizeof request_header_fields / sizeof *request_header_fields);

    return request_header_fields[idx].handler(req, value, e);
}

static status_t
read_request_line(connection conn, http_request request, error e)
{
    char buf[CCH_REQUESTLINE_MAX + 1] = {0};

    if (read_line(conn, buf, sizeof(buf) - 1, e))
        return parse_request_line(buf, request, e);

    /* remap error message to something more meaningful in current context */
    if (is_app_error(e) && get_error_code(e) == ENOSPC)
        set_http_error(e, HTTP_414_REQUEST_URI_TOO_LARGE);

    return failure;
}


/*
 * Reads ONE http request off the socket
 * A http header is terminated by \r\n\r\n
 * Reads the entity from POST as well.
 *
 * We set the connection to persistent if we have V1.1.
 * Then, if Connection: close is specified later,
 * it will be set back to non-persistent
 *
 * We now read all header fields. Check method
 * to see if we need to read an entity body as well.
 */
status_t request_receive(
    http_request request,
    connection conn,
    size_t max_post_content,
    error e)
{
    if (!read_request_line(conn, request, e))
        return failure;

    if (request_get_version(request) == VERSION_11)
        connection_set_persistent(conn, 1);

    if (!read_request_header_fields(conn, request, e))
        return failure;

    if (request_get_method(request) == METHOD_POST && !request->defered_read)
        return read_posted_content(max_post_content, conn, request, e);

    return success;
}

void request_set_connection(http_request request, connection conn)
{
    assert(request != NULL);
    assert(conn != NULL);

    request->external_conn = conn;
}

connection request_get_connection(http_request request)
{
    assert(request != NULL);

    return request->external_conn;
}

void request_set_defered_read(http_request req, int flag)
{
    assert(req != NULL);
    req->defered_read = flag;
}

int request_get_defered_read(http_request req)
{
    assert(req != NULL);
    return req->defered_read;
}
