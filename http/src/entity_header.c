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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <meta_misc.h>

#include <highlander.h>
#include <internals.h>

typedef unsigned long flagtype;

#define ENTITY_HEADER_ALLOW_SET				(0x01)
#define ENTITY_HEADER_CONTENT_ENCODING_SET	(0x02)
#define ENTITY_HEADER_CONTENT_LANGUAGE_SET	(0x04)
#define ENTITY_HEADER_CONTENT_LENGTH_SET	(0x08)
#define ENTITY_HEADER_CONTENT_LOCATION_SET	(0x10)
#define ENTITY_HEADER_CONTENT_MD5_SET		(0x20)
#define ENTITY_HEADER_CONTENT_RANGE_SET		(0x40)
#define ENTITY_HEADER_CONTENT_TYPE_SET		(0x80)
#define ENTITY_HEADER_EXPIRES_SET			(0x100)
#define ENTITY_HEADER_LAST_MODIFIED_SET		(0x200)

struct entity_header_tag {
    flagtype flags;

    cstring allow;
    cstring content_encoding;	/* v1.0 §10.3 v1.1 §14.11 */
    cstring content_language;	/* v1.0 §D.2.5 v1.1 §14.12 */
    size_t content_length;		/* v1.0 §10.4 v1.1 §14.13 */
    cstring content_location;
    cstring content_md5;		/* v1.1 §14.15 */
    cstring content_range;
    cstring content_type;		/* v1.0 §10.5 v1.1 §14.17 */
    time_t expires;
    time_t last_modified;
};

static int entity_header_flag_is_set(entity_header eh, flagtype flag)
{
    assert(eh != NULL);
    assert(flag > 0);

    return eh->flags & flag ? 1 : 0;
}

static void entity_header_set_flag(entity_header eh, flagtype flag)
{
    assert(eh != NULL);
    assert(flag > 0);

    eh->flags |= flag;
}

static void entity_header_clear_flags(entity_header eh)
{
    assert(eh != NULL);
    eh->flags = 0;
}

entity_header entity_header_new(void)
{
    entity_header p;
    cstring arr[7];

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    if (!cstring_multinew(arr, sizeof arr / sizeof *arr)) {
        free(p);
        return NULL;
    }

    entity_header_clear_flags(p);
    p->allow = arr[0];
    p->content_encoding = arr[1];
    p->content_language = arr[2];
    p->content_location = arr[3];
    p->content_md5 = arr[4];
    p->content_type = arr[5];
    p->content_range = arr[6];

    return p;
}

void entity_header_free(entity_header p)
{
    if (p != NULL) {
        cstring_free(p->allow);
        cstring_free(p->content_encoding);
        cstring_free(p->content_language);
        cstring_free(p->content_location);
        cstring_free(p->content_md5);
        cstring_free(p->content_type);
        cstring_free(p->content_range);

        free(p);
    }
}

void entity_header_recycle(entity_header p)
{
    entity_header_clear_flags(p);
    cstring_recycle(p->content_language);
}

status_t entity_header_set_allow(entity_header eh, const char* value)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!cstring_set(eh->allow, value))
        return failure;

    entity_header_set_flag(eh, ENTITY_HEADER_ALLOW_SET);
    return success;
}

void entity_header_set_expires(entity_header eh, time_t value)
{
    assert(eh != NULL);

    eh->expires = value;
    entity_header_set_flag(eh, ENTITY_HEADER_EXPIRES_SET);
}

void entity_header_set_last_modified(entity_header eh, time_t value)
{
    assert(eh != NULL);

    eh->last_modified = value;
    entity_header_set_flag(eh, ENTITY_HEADER_LAST_MODIFIED_SET);
}

status_t entity_header_set_content_language(entity_header eh, const char* value, meta_error e)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!cstring_set(eh->content_language, value))
        return set_os_error(e, errno);

    entity_header_set_flag(eh, ENTITY_HEADER_CONTENT_LANGUAGE_SET);
    return success;
}

void entity_header_set_content_length(entity_header eh, const size_t value)
{
    assert(eh != NULL);

    eh->content_length = value;
    entity_header_set_flag(eh, ENTITY_HEADER_CONTENT_LENGTH_SET);
}

status_t entity_header_set_content_encoding(entity_header eh, const char* value)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!cstring_set(eh->content_encoding, value))
        return failure;

    entity_header_set_flag(eh, ENTITY_HEADER_CONTENT_ENCODING_SET);
    return success;
}

status_t entity_header_set_content_type(entity_header eh, const char* value)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!cstring_set(eh->content_type, value))
        return failure;

    entity_header_set_flag(eh, ENTITY_HEADER_CONTENT_TYPE_SET);
    return success;
}


status_t entity_header_set_content_md5(entity_header eh, const char* value)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!cstring_set(eh->content_md5, value))
        return failure;

    entity_header_set_flag(eh, ENTITY_HEADER_CONTENT_MD5_SET);
    return success;
}

status_t entity_header_set_content_location(entity_header eh, const char* value)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!cstring_set(eh->content_location, value))
        return failure;

    entity_header_set_flag(eh, ENTITY_HEADER_CONTENT_LOCATION_SET);
    return success;
}

status_t entity_header_set_content_range(entity_header eh, const char* value)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!cstring_set(eh->content_range, value))
        return failure;

    entity_header_set_flag(eh, ENTITY_HEADER_CONTENT_RANGE_SET);
    return success;
}

bool entity_header_content_type_is(entity_header eh, const char* val)
{
    return !cstring_compare(eh->content_type, val);
}

bool entity_header_allow_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_ALLOW_SET);
}

bool entity_header_content_encoding_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_ENCODING_SET);
}

bool entity_header_content_language_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LANGUAGE_SET);
}

bool entity_header_content_length_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LENGTH_SET);
}

bool entity_header_content_location_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LOCATION_SET);
}

bool entity_header_content_md5_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_MD5_SET);
}

bool entity_header_content_range_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_RANGE_SET);
}

bool entity_header_content_type_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_TYPE_SET);
}

bool entity_header_expires_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_EXPIRES_SET);
}

bool entity_header_last_modified_isset(entity_header eh)
{
    assert(eh != NULL);
    return entity_header_flag_is_set(eh, ENTITY_HEADER_LAST_MODIFIED_SET);
}

const char* entity_header_get_allow(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_ALLOW_SET));
    return c_str(eh->allow);
}

const char* entity_header_get_content_encoding(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_ENCODING_SET));
    return c_str(eh->content_encoding);
}

const char* entity_header_get_content_language(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LANGUAGE_SET));
    return c_str(eh->content_language);
}

size_t entity_header_get_content_length(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LENGTH_SET));
    return eh->content_length;
}

const char* entity_header_get_content_location(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LOCATION_SET));
    return c_str(eh->content_location);
}

const char* entity_header_get_content_md5(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_MD5_SET));
    return c_str(eh->content_md5);
}

const char* entity_header_get_content_range(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_RANGE_SET));
    return c_str(eh->content_range);
}

const char* entity_header_get_content_type(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_TYPE_SET));
    return c_str(eh->content_type);
}

time_t entity_header_get_expires(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_EXPIRES_SET));
    return eh->expires;
}

time_t entity_header_get_last_modified(entity_header eh)
{
    assert(eh != NULL);
    assert(entity_header_flag_is_set(eh, ENTITY_HEADER_LAST_MODIFIED_SET));
    return eh->last_modified;
}

static inline status_t send_allow(entity_header eh, connection conn)
{
    return http_send_field(conn, "Allow: ", eh->allow);
}

static inline status_t send_content_encoding(entity_header eh, connection conn)
{
    return http_send_field(conn, "Content-Encoding: ", eh->content_encoding);
}

static inline status_t send_content_language(entity_header eh, connection conn)
{
    return http_send_field(conn, "Content-Language: ", eh->content_language);
}

static inline status_t send_content_location(entity_header eh, connection conn)
{
    return http_send_field(conn, "Content-Location: ", eh->content_location);
}

static inline status_t send_content_md5(entity_header eh, connection conn)
{
    return http_send_field(conn, "Content-MD5: ", eh->content_md5);
}

static inline status_t send_content_range(entity_header eh, connection conn)
{
    return http_send_field(conn, "Content-Range: ", eh->content_range);
}

static inline status_t send_content_type(entity_header eh, connection conn)
{
    return http_send_field(conn, "Content-Type: ", eh->content_type);
}

static inline status_t send_expires(entity_header eh, connection conn)
{
    return http_send_date(conn, "Expires: ", eh->expires);
}

static inline status_t send_last_modified(entity_header eh, connection conn)
{
    return http_send_date(conn, "Last-Modified: ", eh->last_modified);
}

static inline status_t send_content_length(entity_header eh, connection conn)
{
    char buf[100] = {'\0'}; /* "Content-Length: " + length + '\r\n\0' */
    size_t cb, content_length = 0;

    content_length = entity_header_get_content_length(eh);
    sprintf(buf, "Content-Length: %lu\r\n", (unsigned long)content_length);
    cb = strlen(buf);
    return connection_write(conn, buf, cb);
}

status_t entity_header_send_fields(entity_header eh, connection c)
{
    status_t rc = success;
    size_t i, n;

    static const struct {
        size_t flag;
        status_t (*func)(entity_header, connection);
    } fields[] = {
        { ENTITY_HEADER_ALLOW_SET,				send_allow },
        { ENTITY_HEADER_CONTENT_ENCODING_SET,	send_content_encoding },
        { ENTITY_HEADER_CONTENT_LANGUAGE_SET,	send_content_language },
        { ENTITY_HEADER_CONTENT_LENGTH_SET,		send_content_length },
        { ENTITY_HEADER_CONTENT_LOCATION_SET,	send_content_location },
        { ENTITY_HEADER_CONTENT_MD5_SET,		send_content_md5 },
        { ENTITY_HEADER_CONTENT_RANGE_SET,		send_content_range },
        { ENTITY_HEADER_CONTENT_TYPE_SET,		send_content_type },
        { ENTITY_HEADER_EXPIRES_SET,			send_expires },
        { ENTITY_HEADER_LAST_MODIFIED_SET,		send_last_modified },
    };

    n = sizeof fields / sizeof *fields;
    for (i = 0; i < n; i++) {
        if (entity_header_flag_is_set(eh, fields[i].flag))
            if ((rc = fields[i].func(eh, c)) == failure)
                break;
    }

    return rc;

}

/* Parsing functions */
/* Entity header handlers */

static status_t parse_content_encoding(entity_header eh, const char* value, meta_error e)
{
    assert(eh != NULL);
    assert(value != NULL);
    /*
     * §14.11
     * Used as a modifier to the Content-Type
     * a) Reply with 415 if the encoding type isn't acceptable
     * b) If multiple encodings has been applied, they must be
     * listed in the order they were applied.
     *
     * Typical: "Content-Encoding: gzip"
     * See §3.5 for definition, the basic point is that gzip, compress, deflate is OK.
     */
    if (!entity_header_set_content_encoding(eh, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_content_length(entity_header eh, const char* value, meta_error e)
{
    size_t len;
    /*
     * §14.13
     * a) Legal values are >= 0
     * b) May be prohibited by §4.4
     * c) See §4.4 if Content-Length is missing
     */

    /*
     * We do a "manual" conversion here instead of using
     * e.g. strtoul(), since strtoul() 1) removes WS and 2) stops
     * if it finds non-digits. We require digits in all bytes.
     */
    assert(eh != NULL);
    assert(value != NULL);

    if (!string2size_t(value, &len))
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    entity_header_set_content_length(eh, (unsigned long)len);
    return success;
}

static status_t parse_content_md5(entity_header eh, const char* value, meta_error e)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!entity_header_set_allow(eh, value))
        return set_os_error(e, errno);

    return success;
}

/* Helper function to have the algorithm one place only */
static status_t eh_parse_multivalued_fields(
    void *dest,
    const char* value,
    status_t(*set_func)(entity_header dest, const char* value, meta_error e),
    meta_error e)
{
    const int sep = ',';
    char buf[100];
    char* s;

    assert(dest != NULL);
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
        if (!set_func(dest, buf, e))
            return 0;

        value = s + 1;
    }

    return set_func(dest, value, e);
}

/*
 * Notes: The language tags are defined in RFC1766, and there are 
 * too many to check.
 * Anything goes, IOW.
 */
static status_t parse_content_language(entity_header eh, const char* value, meta_error e)
{
    assert(eh != NULL);
    assert(value != NULL);

    /*
     * NOTE: If we receive a document with content-language, then we
     * MUST remember to store that information somewhere!
     */
    return eh_parse_multivalued_fields(eh, value, entity_header_set_content_language, e);
}

static status_t parse_allow(entity_header eh, const char* value, meta_error e)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!entity_header_set_allow(eh, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_content_location(entity_header eh, const char* value, meta_error e)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!entity_header_set_content_location(eh, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_content_range(entity_header eh, const char* value, meta_error e)
{
    assert(eh != NULL);
    assert(value != NULL);

    if (!entity_header_set_content_range(eh, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_content_type(entity_header eh, const char* value, meta_error e)
{
    if (!entity_header_set_content_type(eh, value))
        return set_os_error(e, errno);

    return success;
}

static status_t parse_expires(entity_header eh, const char* value, meta_error e)
{
    time_t t;

    assert(eh != NULL);
    assert(value != NULL);

    if ((t = parse_rfc822_date(value)) == (time_t)-1)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    entity_header_set_expires(eh, t);
    return success;
}

static status_t parse_last_modified(entity_header eh, const char* value, meta_error e)
{
    time_t t;

    assert(eh != NULL);
    assert(value != NULL);

    if ((t = parse_rfc822_date(value)) == (time_t)-1)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    entity_header_set_last_modified(eh, t);
    return success;
}

static const struct {
    const char* name;
    status_t (*handler)(entity_header eh, const char* value, meta_error e);
} entity_header_fields[] = {
    { "allow",				parse_allow },
    { "content-encoding",	parse_content_encoding },
    { "content-language",	parse_content_language },
    { "content-length",		parse_content_length },
    { "content-location",	parse_content_location },
    { "content-md5",		parse_content_md5 },
    { "content-range",		parse_content_range },
    { "content-type",		parse_content_type },
    { "expires",			parse_expires },
    { "last-modified",		parse_last_modified },
};

status_t parse_entity_header(int idx, entity_header gh, const char* value, meta_error e)
{
    assert(idx >= 0);
    assert((size_t)idx < sizeof entity_header_fields / sizeof *entity_header_fields);

    return entity_header_fields[idx].handler(gh, value, e);
}


int find_entity_header(const char* name)
{
    int i, n = sizeof entity_header_fields / sizeof *entity_header_fields;

    for (i = 0; i < n; i++) {
        if (strcmp(entity_header_fields[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

int entity_header_dump(entity_header eh, void *file)
{
    FILE *f = file;
    char datebuf[100];

    assert(eh != NULL);
    assert(f != NULL);

    fprintf(f, "Entity header fields\n");

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_ALLOW_SET))
        fprintf(f, "\tallow: %s\n", c_str(eh->allow));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_ENCODING_SET))
        fprintf(f, "\tcontent_encoding: %s\n", c_str(eh->content_encoding));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LANGUAGE_SET))
        fprintf(f, "\tcontent_language: %s\n", c_str(eh->content_language));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LENGTH_SET))
        fprintf(f, "\tcontent_length: %zu\n", eh->content_length);

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_LOCATION_SET))
        fprintf(f, "\tcontent_location: %s\n", c_str(eh->content_location));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_MD5_SET))
        fprintf(f, "\tcontent_md5: %s\n", c_str(eh->content_md5));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_RANGE_SET))
        fprintf(f, "\tcontent_range: %s\n", c_str(eh->content_range));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_CONTENT_TYPE_SET))
        fprintf(f, "\tcontent_type: %s\n", c_str(eh->content_type));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_EXPIRES_SET))
        fprintf(f, "\texpires: %s", ctime_r(&eh->expires, datebuf));

    if (entity_header_flag_is_set(eh, ENTITY_HEADER_LAST_MODIFIED_SET))
        fprintf(f, "\tlast_modified: %s", ctime_r(&eh->last_modified, datebuf));

    return 1;
    }
