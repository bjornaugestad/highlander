/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */


#ifndef INTERNALS_H
#define INTERNALS_H

#include <cstring.h>
#include <connection.h>
#include <meta_error.h>
#include <meta_common.h>

#include <http_server.h>
#if 0
#include <highlander.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x)	(void)x

// Max length of version part, HTTP/x.x . 20 should do for a while...
#define CCH_VERSION_MAX	20

// Max len of requested URI
// This includes any parameters given,
// like foo.html?bar=foobar&f=asdasdasdasdasdasd
// I guess this can become very large?
#define CCH_URI_MAX 10240

// The max # of characters for a http header fieldname.
// The longest in http 1.1 is If-Unmodified-Since which is 19 bytes (+ 1 for '\0')
#define CCH_FIELDNAME_MAX	30
#define CCH_FIELDVALUE_MAX	10000


status_t http_send_field(connection conn, const char *name, cstring value)
    __attribute__((nonnull, warn_unused_result));
status_t http_send_date(connection conn, const char *name, time_t value)
    __attribute__((nonnull, warn_unused_result));
status_t http_send_ulong(connection conn, const char *name, unsigned long value)
    __attribute__((nonnull, warn_unused_result));
status_t http_send_int(connection conn, const char *name, int value)
    __attribute__((nonnull, warn_unused_result));
status_t http_send_unsigned_int(connection conn, const char *name, unsigned int value)
    __attribute__((nonnull, warn_unused_result));
status_t http_send_string(connection conn, const char *s)
    __attribute__((nonnull, warn_unused_result));

status_t read_line(connection conn, char *buf, size_t cchMax, error e)
    __attribute__((nonnull, warn_unused_result));
/*
 * A field name, in HTTP, is everything to the left of : in
 * header fields like "name: value". We copy the name part here.
 */
status_t get_field_name(const char *buf, char *name, size_t cchNameMax)
    __attribute__((nonnull, warn_unused_result));
status_t get_field_value(const char *buf, char *value, size_t cchValueMax)
    __attribute__((nonnull, warn_unused_result));


dynamic_page dynamic_new(const char *uri, handlerfn f, page_attribute a);
void dynamic_free(dynamic_page p);
void dynamic_set_handler(dynamic_page p, handlerfn func);
status_t dynamic_set_uri(dynamic_page p, const char *value)
    __attribute__((nonnull, warn_unused_result));
int dynamic_run(dynamic_page p, const http_request, http_response);
status_t dynamic_set_attributes(dynamic_page p, page_attribute a)
    __attribute__((nonnull, warn_unused_result));
page_attribute dynamic_get_attributes(dynamic_page p);
const char*	dynamic_get_uri(dynamic_page p);


#ifndef CHOPPED

/* Function prototypes for handler functions */
status_t parse_cookie(http_request r, const char *s, error e)
    __attribute__((nonnull, warn_unused_result));

status_t parse_new_cookie(http_request r, const char *s, error e)
    __attribute__((nonnull, warn_unused_result));

status_t parse_old_cookie(http_request r, const char *s, error e)
    __attribute__((nonnull, warn_unused_result));


#endif

time_t parse_rfc822_date(const char *s);
status_t send_status_code(connection conn, int status_code,
    int version) __attribute__((nonnull, warn_unused_result));

int http_status_code(int error);


status_t handle_dynamic(http_server srv, dynamic_page p,
    http_request req, http_response response, error e) __attribute__((nonnull, warn_unused_result));

status_t parse_request_headerfield(connection conn, const char *fieldname,
    const char *value, http_request req, error e) __attribute__((nonnull, warn_unused_result));

status_t parse_response_headerfield(const char *name, const char *value,
    http_response req, error e) __attribute__((nonnull, warn_unused_result));

int find_request_header(const char *name);
status_t parse_request_header(int idx, http_request req, const char *value, error e) __attribute__((nonnull, warn_unused_result));

int find_response_header(const char *name);
status_t parse_response_header(int idx, http_response req,
    const char *value, error e)
    __attribute__((nonnull, warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
