/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef INTERNALS_H
#define INTERNALS_H

#include <stdbool.h>

#include <cstring.h>
#include <connection.h>
#include <meta_error.h>
#include <meta_common.h>

#include <http_request.h>
#include <http_response.h>

#ifdef __cplusplus
extern "C" {
#endif

// Max length of version part, HTTP/x.x . 20 should do for a while...
#define CCH_VERSION_MAX	20

// Max len of requested URI
// This includes any parameters given,
// like foo.html?bar=foobar&f=asdasdasdasdasdasd
// I guess this can become very large?
#define CCH_URI_MAX 10240

// The max # of characters for a http header fieldname.
// The longest in http 1.1 is If-Unmodified-Since which is 19 bytes (+ 1 for '\0')
// Bump field name for ... reasons.
#define CCH_FIELDNAME_MAX	64
#define CCH_FIELDVALUE_MAX	8192


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


time_t parse_rfc822_date(const char *s);
status_t send_status_code(connection conn, int status_code,
    int version) __attribute__((nonnull, warn_unused_result));


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
