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


#ifndef INTERNALS_H
#define INTERNALS_H

#include <cstring.h>
#include <connection.h>
#include <meta_error.h>
#include <meta_common.h>

#include <highlander.h>
#ifdef __cplusplus
extern "C" {
#endif

#define UNUSED(x)	(void)x

#define EFS_INTERNAL		8	/* Internal server error */
#define EFS_UNKNOWN_HEADER_FIELD 1000

/*
 * Params are here parameters to the uri
 */
#define CCH_PARAMNAME_MAX	100
#define CCH_PARAMVALUE_MAX	500

/*
 * Max len of a request keyword, e.g. GET, HEAD
 */
#define CCH_METHOD_MAX	256

/*
 * Max length of version part, HTTP/x.x . 20 should do for a while...
 */
#define CCH_VERSION_MAX	20

/*
 * Max len of requested URI
 * This includes any parameters given,
 * like foo.html?bar=foobar&f=asdasdasdasdasdasd
 * I guess this can become very large?
 */
#define CCH_URI_MAX 10240

/*
 *
 * The max # of characters for a http header fieldname.
 * The longest in http 1.1 is If-Unmodified-Since which is 19 bytes (+ 1 for '\0')
 */
#define CCH_FIELDNAME_MAX	30
#define CCH_FIELDVALUE_MAX	10000

/*
 * Quality is not used in http 1.0.
 * It is used in conjunction with accept-xxx to prioritize the different formats
 * a client accepts. My guess is that it is not used a lot... We support it anyway.
 * The format is q=float where float is limited by me to be 0.nnn
 * This means that we need 8 bytes. We safe a little and size for 12.
 */
#define CCH_QUALITY_MAX		12

/* Max len of a request line, regardless of version */
#define CCH_REQUESTLINE_MAX 10240

/* Max length of a HTTP status line */
#define CCH_STATUSLINE_MAX 256

/*
 * Max length of a language, as in en, no, se
 * These are defined by IANA
 */
#define CCH_LANGUAGE_MAX	100

/* Magic value for cookies */
#define MAX_AGE_NOT_SET -1

void *serviceConnection(void *psa);

dynamic_page http_server_lookup(http_server srv, http_request request);
http_request http_server_get_request(http_server srv);
http_response http_server_get_response(http_server srv);
page_attribute http_server_get_default_attributes(http_server srv);
void http_server_recycle_request(http_server srv, http_request request);
void http_server_recycle_response(http_server srv, http_response response);
void http_server_add_logentry(http_server srv, connection conn, http_request req, int sc, size_t cb);

/* Handles the default page */
int http_server_has_default_page_handler(http_server s);
status_t http_server_run_default_page_handler(connection conn, http_server s, http_request request, http_response response, meta_error e);
status_t http_send_field(connection conn, const char* name, cstring value);
status_t http_send_date(connection conn, const char* name, time_t value);
status_t http_send_ulong(connection conn, const char* name, unsigned long value);
status_t http_send_string(connection conn, const char* s);

status_t read_line(connection conn, char* buf, size_t cchMax, meta_error e);
/*
 * A field name, in HTTP, is everything to the left of : in
 * header fields like "name: value". We copy the name part here.
 */
status_t get_field_name(const char* buf, char* name, size_t cchNameMax);
status_t get_field_value(const char* buf, char* value, size_t cchValueMax);


dynamic_page dynamic_new(const char *uri, PAGE_FUNCTION f, page_attribute a);
void dynamic_free(dynamic_page p);
void dynamic_set_handler(dynamic_page p, PAGE_FUNCTION func);
status_t dynamic_set_uri(dynamic_page p, const char *value);
int dynamic_run(dynamic_page p, const http_request, http_response);
int dynamic_set_attributes(dynamic_page p, page_attribute a);
page_attribute dynamic_get_attributes(dynamic_page p);
const char*	dynamic_get_uri(dynamic_page p);


page_attribute attribute_dup(page_attribute a);

void cookie_free(cookie c);

time_t parse_rfc822_date(const char *s);
status_t send_status_code(
	connection conn,
	int status_code,
	http_version version);

int http_status_code(int error);


status_t handle_dynamic(
	connection conn,
	http_server srv,
	dynamic_page p,
	http_request req,
	http_response response,
	meta_error e);

general_header general_header_new(void);
void general_header_free(general_header p);
void general_header_recycle(general_header p);
status_t general_header_send_fields(general_header gh, connection c);


void general_header_set_no_cache(general_header gh);
void general_header_set_no_store(general_header gh);
void general_header_set_max_age(general_header gh, int value);
void general_header_set_s_maxage(general_header gh, int value);
void general_header_set_max_stale(general_header gh, int value);
void general_header_set_min_fresh(general_header gh, int value);
void general_header_set_no_transform(general_header gh);
void general_header_set_only_if_cached(general_header gh);

void general_header_set_private(general_header gh);
void general_header_set_public(general_header gh);
void general_header_set_must_revalidate(general_header gh);
void general_header_set_proxy_revalidate(general_header gh);

void general_header_set_date(general_header gh, time_t value);
status_t general_header_set_connection(general_header gh, const char *value);
status_t general_header_set_pragma(general_header gh, const char* value);
status_t general_header_set_trailer(general_header gh, const char *value);
status_t general_header_set_transfer_encoding(general_header gh, const char *value);
status_t general_header_set_upgrade(general_header gh, const char *value);
status_t general_header_set_via(general_header gh, const char *value);
status_t general_header_set_warning(general_header gh, const char *value);

int general_header_get_no_cache(general_header gh);
int general_header_get_no_store(general_header gh);
int general_header_get_max_age(general_header gh);
int general_header_get_s_maxage(general_header gh);
int general_header_get_max_stale(general_header gh);
int general_header_get_min_fresh(general_header gh);
int general_header_get_no_transform(general_header gh);
int general_header_get_only_if_cached(general_header gh);
int general_header_get_public(general_header gh);
int general_header_get_private(general_header gh);
int general_header_get_must_revalidate(general_header gh);
int general_header_get_proxy_revalidate(general_header gh);


time_t general_header_get_date(general_header gh);
const char* general_header_get_connection(general_header gh);
const char* general_header_get_trailer(general_header gh);
const char* general_header_get_transfer_encoding(general_header gh);
const char* general_header_get_upgrade(general_header gh);
const char* general_header_get_via(general_header gh);
const char* general_header_get_warning(general_header gh);

/* Tests if a property is set or not. Use it before calling the _get() functions */
int general_header_no_cache_isset(general_header gh);
int general_header_no_store_isset(general_header gh);
int general_header_max_age_isset(general_header gh);
int general_header_connection_isset(general_header gh);
int general_header_pragma_isset(general_header gh);
int general_header_max_stale_isset(general_header gh);
int general_header_min_fresh_isset(general_header gh);
int general_header_no_transform_isset(general_header gh);
int general_header_only_if_cached_isset(general_header gh);
int general_header_date_isset(general_header gh);
int general_header_trailer_isset(general_header gh);
int general_header_transfer_encoding_isset(general_header gh);
int general_header_upgrade_isset(general_header gh);
int general_header_via_isset(general_header gh);
int general_header_warning_isset(general_header gh);


entity_header entity_header_new(void);
void entity_header_free(entity_header p);
void entity_header_recycle(entity_header p);
status_t entity_header_send_fields(entity_header eh, connection c);

status_t entity_header_set_allow(entity_header eh, const char *value);
status_t entity_header_set_content_encoding(entity_header eh, const char *value);
status_t entity_header_set_content_language(entity_header eh, const char *value, meta_error e);
void entity_header_set_content_length(entity_header eh, size_t value);
status_t entity_header_set_content_location(entity_header eh, const char *value);
status_t entity_header_set_content_md5(entity_header eh, const char *value);
status_t entity_header_set_content_range(entity_header eh, const char *value);
status_t entity_header_set_content_type(entity_header eh, const char *value);
void entity_header_set_expires(entity_header eh, time_t value);
void entity_header_set_last_modified(entity_header eh, time_t value);

int entity_header_content_type_is(entity_header eh, const char* val);

const char* entity_header_get_allow(entity_header eh);
const char* entity_header_get_content_encoding(entity_header eh);
const char* entity_header_get_content_language(entity_header eh);
size_t entity_header_get_content_length(entity_header eh);
const char* entity_header_get_content_location(entity_header eh);
const char* entity_header_get_content_md5(entity_header eh);
const char* entity_header_get_content_range(entity_header eh);
const char* entity_header_get_content_type(entity_header eh);
time_t entity_header_get_expires(entity_header eh);
time_t entity_header_get_last_modified(entity_header eh);

int entity_header_allow_isset(entity_header eh);
int entity_header_content_encoding_isset(entity_header eh);
int entity_header_content_language_isset(entity_header eh);
int entity_header_content_length_isset(entity_header eh);
int entity_header_content_location_isset(entity_header eh);
int entity_header_content_md5_isset(entity_header eh);
int entity_header_content_range_isset(entity_header eh);
int entity_header_content_type_isset(entity_header eh);
int entity_header_expires_isset(entity_header eh);
int entity_header_last_modified_isset(entity_header eh);

void request_set_connection(http_request req, connection c);
status_t parse_request_headerfield(
	connection conn,
	const char *fieldname,
	const char *value,
	http_request req,
	meta_error e);

status_t parse_response_headerfield(
	const char* name,
	const char* value,
	http_response req,
	meta_error e);

int parse_multivalued_fields(
	void *dest,
	const char* value,
	int(*set_func)(void *dest, const char* value, meta_error e),
	meta_error e);

/* Return an index in the entity header array,
 * or -1 if the field was not found. */
int find_entity_header(const char* name);
status_t parse_entity_header(int idx, entity_header gh, const char* value, meta_error e);

/*
 * The general header fields are shared between http requests and http
 * responses. The fields are described in RFC 2616, section 4.5.
 * This function itself returns -1 if the field was NOT a general header field
 */
int find_general_header(const char* name);
status_t parse_general_header(int idx, general_header gh, const char* value, meta_error e);

int find_request_header(const char* name);
status_t parse_request_header(int idx, http_request req, const char* value, meta_error e);

int find_response_header(const char* name);
status_t parse_response_header(int idx, http_response req, const char* value, meta_error e);

/* Function prototypes for handler functions */
status_t parse_cookie(http_request r, const char* s, meta_error e);
status_t parse_new_cookie(http_request r, const char* s, meta_error e);
status_t parse_old_cookie(http_request r, const char* s, meta_error e);
status_t cookie_dump(cookie c,  void *file);

#ifdef __cplusplus
}
#endif

#endif
