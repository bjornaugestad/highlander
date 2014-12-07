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


#ifndef HIGHLANDER_H
#define HIGHLANDER_H

/* System files we need */
#include <time.h> /* for time_t */

#include <meta_process.h>
#include <meta_error.h>
#include <connection.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_100_CONTINUE 100
#define HTTP_101_SWITCHING_PROTOCOLS 101
#define HTTP_200_OK 200
#define HTTP_201_CREATED 201
#define HTTP_202_ACCEPTED 202
#define HTTP_203_NON_AUTHORATIVE_INFORMATION 203
#define HTTP_204_NO_CONTENT 204
#define HTTP_205_RESET_CONTENT 205
#define HTTP_206_PARTIAL_CONTENT 206
#define HTTP_300_MULTIPLE_CHOICES 300
#define HTTP_301_MOVED_PERMANENTLY 301
#define HTTP_302_FOUND 302
#define HTTP_303_SEE_OTHER 303
#define HTTP_304_NOT_MODIFIED 304
#define HTTP_305_USE_PROXY 305
#define HTTP_307_TEMPORARY_REDIRECT 307
#define HTTP_400_BAD_REQUEST 400
#define HTTP_401_UNAUTHORIZED 401
#define HTTP_402_PAYMENT_REQUIRED 402
#define HTTP_403_FORBIDDEN 403
#define HTTP_404_NOT_FOUND 404
#define HTTP_405_METHOD_NOT_ALLOWED 405
#define HTTP_406_NOT_ACCEPTABLE 406
#define HTTP_407_PROXY_AUTHENTICATION_REQUIRED 407
#define HTTP_408_REQUEST_TIMEOUT 408
#define HTTP_409_CONFLICT 409
#define HTTP_410_GONE 410
#define HTTP_411_LENGTH_REQUIRED 411
#define HTTP_412_PRECONDITION_FAILED 412
#define HTTP_413_REQUEST_ENTITY_TOO_LARGE 413
#define HTTP_414_REQUEST_URI_TOO_LARGE 414
#define HTTP_415_UNSUPPORTED_MEDIA_TYPE 415
#define HTTP_416_REQUESTED_RANGE_NOT_SATISFIABLE 416
#define HTTP_417_EXPECTATION_FAILED 417
#define HTTP_500_INTERNAL_SERVER_ERROR 500
#define HTTP_501_NOT_IMPLEMENTED 501
#define HTTP_502_BAD_GATEWAY 502
#define HTTP_503_SERVICE_UNAVAILABLE 503
#define HTTP_504_GATEWAY_TIME_OUT 504
#define HTTP_505_HTTP_VERSION_NOT_SUPPORTED 505

#define HTTP_STATUS_MIN 100
#define HTTP_STATUS_MAX 505


#define LOGFILE_MAX	10240
#define DOCUMENTROOT_MAX 10240

/* The request methods we support */
enum http_method {METHOD_UNKNOWN, METHOD_GET, METHOD_HEAD, METHOD_POST};

/* the http versions we support */
enum http_version {VERSION_UNKNOWN, VERSION_09, VERSION_10, VERSION_11};

typedef struct http_server_tag*		http_server;
typedef struct http_request_tag*	http_request;
typedef struct http_response_tag*	http_response;
typedef enum http_method			http_method;
typedef enum http_version			http_version;
typedef struct cookie_tag*			cookie;
typedef struct dynamic_page_tag*	dynamic_page;
typedef struct page_attribute_tag*	page_attribute;
typedef struct general_header_tag*	general_header;
typedef struct entity_header_tag*	entity_header;

typedef int (*PAGE_FUNCTION)(http_request, http_response);


http_server http_server_new(void);
void http_server_free(http_server s);

int http_server_configure(http_server s, process p, const char* filename);
int http_server_start_via_process(process p, http_server s);
void http_server_set_can_read_files(http_server s, int val);
int http_server_can_read_files(http_server s);
int http_server_alloc(http_server s);
int http_server_get_root_resources(http_server s);
int http_server_free_root_resources(http_server s);
int http_server_start(http_server srv);
int http_server_shutting_down(http_server s);
int http_server_shutdown(http_server s);
int http_server_add_page(http_server s, const char* uri, PAGE_FUNCTION pf, page_attribute attr);
void http_server_trace(http_server s, int level);

void http_server_set_defered_read(http_server s, int flag);
int http_server_get_defered_read(http_server s);

int	   http_server_set_documentroot(http_server s, const char* docroot);
void   http_server_set_post_limit(http_server s, size_t cb);
size_t http_server_get_post_limit(http_server s);
const char* http_server_get_documentroot(http_server s);

void http_server_set_default_page_handler(http_server s, PAGE_FUNCTION pf);
int http_server_set_default_page_attributes(http_server s, page_attribute a);
int http_server_set_host(http_server s, const char* name);
void http_server_set_port(http_server s, int n);

int http_server_get_timeout_write(http_server s);
int http_server_get_timeout_read(http_server srv);
int http_server_get_timeout_accept(http_server srv);
int http_server_get_timeout_read(http_server s);

void http_server_set_timeout_write(http_server s, int seconds);
void http_server_set_timeout_read(http_server s, int seconds);
void http_server_set_timeout_accept(http_server s, int seconds);
void http_server_set_retries_read(http_server s, int seconds);
void http_server_set_retries_write(http_server s, int seconds);

void http_server_set_worker_threads(http_server s, size_t n);
void http_server_set_queue_size(http_server s, size_t n);
void http_server_set_max_pages(http_server s, size_t n);
void http_server_set_block_when_full(http_server s, int n);
int	 http_server_set_logfile(http_server s, const char *logfile);
void http_server_set_logrotate(http_server s, int logrotate);

int http_server_get_port(http_server s);

size_t http_server_get_worker_threads(http_server s);
size_t http_server_get_queue_size(http_server s);
int	   http_server_get_block_when_full(http_server s);
size_t http_server_get_max_pages(http_server s);

const char* request_get_uri(http_request request);
const char* request_get_referer(http_request p);
http_method request_get_method(http_request request);
http_version request_get_version(http_request request);
const char* request_get_host(http_request request);
const char* request_get_content(http_request request);
size_t		request_get_content_length(http_request request);

/* request arguments */
int			request_get_parameter_count(http_request request);
const char* request_get_parameter_name (http_request request, size_t i);
const char* request_get_parameter_value(http_request request, const char* name);

/* Forms */
size_t request_get_field_count(http_request request);
size_t request_get_field_namelen(http_request request, size_t i);
size_t request_get_field_valuelen(http_request request, size_t i);
int	   request_get_field_name(http_request request, size_t i, char *s, size_t cb);
int	   request_get_field_value(http_request request, size_t i, char *s, size_t cb);
int	   request_get_field_value_by_name(http_request request, const char* name, char *value, size_t cb);

/* request cookies */
size_t request_get_cookie_count(http_request request);
cookie request_get_cookie(http_request request, size_t i);

int request_content_type_is(http_request request, const char* val);
int request_accepts_media_type(http_request request, const char* val);
int request_accepts_language(http_request request, const char* lang);
const char* request_get_content_type(http_request request);
const char* request_get_user_agent(http_request request);

/* Returns the HTTP header field if_modified_since or (time_t)-1
 * if the field wasn't set.
 */
time_t request_get_if_modified_since(http_request r);

/* Cookies */
cookie cookie_new(void);

int	 cookie_set_name(cookie c, const char* s);
int	 cookie_set_value(cookie c, const char* s);
int	 cookie_set_comment(cookie c, const char* s);
int	 cookie_set_domain(cookie c, const char* s);
int	 cookie_set_path(cookie c, const char* s);
int	 cookie_set_max_age(cookie c, int value);
void cookie_set_version(cookie c, int value);
void cookie_set_secure(cookie c, int value);

const char*	cookie_get_name(cookie c);
const char*	cookie_get_value(cookie c);
const char*	cookie_get_comment(cookie c);
const char*	cookie_get_domain(cookie c);
const char*	cookie_get_path(cookie c);
int cookie_get_version(cookie c);
int cookie_get_secure(cookie c);
int cookie_get_max_age(cookie c);
connection request_get_connection(http_request req);
void request_set_defered_read(http_request req, int flag);
int request_get_defered_read(http_request req);

http_request request_new(void);
void request_free(http_request p);

/* returns 0 on memory errors, else 1 */
int request_set_entity(http_request r, void *entity, size_t cb);

void request_set_version(http_request r, http_version version);
void request_recycle(http_request r);

/* @return 0 for success or error code for failure (ENOMEM) */
int request_add_param(http_request r, const char *name, const char *value);
void request_set_method(http_request r, http_method method);

/* @return 0 for success  or error code indicating failure(ENOMEM). */
int request_set_uri(http_request r, const char *value);
int request_set_mime_version(http_request request, int major, int minor, meta_error e);

int request_set_accept_ranges(http_request r, const char *value);
int request_set_age(http_request r, unsigned long value);
int request_set_etag(http_request r, const char *value);
int request_set_location(http_request r, const char *value);
int request_set_proxy_authenticate(http_request r, const char *value);
int request_set_retry_after(http_request r, const char *value);
int request_set_server(http_request r, const char *value);
int request_set_vary(http_request r, const char *value);
int request_set_www_authenticate(http_request r, const char *value);
int request_set_accept(http_request r, const char *value, meta_error e);
int request_set_accept_charset(http_request r, const char *value, meta_error e);
int request_set_accept_encoding(http_request r, const char *value, meta_error e);
int request_set_accept_language(http_request r, const char *value, meta_error e);
int request_set_authorization(http_request r, const char *value);
int request_set_expect(http_request r, const char *value);
int request_set_from(http_request r, const char *value);
int request_set_host(http_request r, const char *value);
int request_set_if_match(http_request r, const char *value);
void request_set_if_modified_since(http_request r, time_t value);
int request_set_if_none_match(http_request r, const char *value);
int request_set_if_range(http_request r, const char *value);
int request_set_if_unmodified_since(http_request r, time_t value);
void request_set_max_forwards(http_request r, unsigned long value);
int request_set_proxy_authorization(http_request r, const char *value);
int request_set_range(http_request r, const char *value);
int request_set_referer(http_request r, const char *value);
int request_set_te(http_request r, const char *value, meta_error e);
int request_set_user_agent(http_request r, const char *value);
int request_add_cookie(http_request r, cookie c);
int request_send(http_request r, connection c, meta_error e);
int request_receive(http_request r, connection c, size_t max_posted_content, meta_error e);

http_response response_new(void);
void response_free(http_response p);
void response_recycle(http_response p);
void response_set_version(http_response r, http_version version);
void response_set_status(http_response r, int status);
int response_get_status(http_response r);

general_header request_get_general_header(http_request r);
entity_header request_get_entity_header(http_request r);

general_header response_get_general_header(http_response r);
entity_header response_get_entity_header(http_response r);

/* Note that file argument *MUST* be a FILE* */
int response_dump(http_response r, void *file);
int general_header_dump(general_header gh, void *file);
int entity_header_dump(entity_header gh, void *file);

const char* response_get_connection(http_response response);
const char*	response_get_entity(http_response p);
size_t response_get_content_length(http_response p);

int response_set_cookie(http_response response, cookie c);
int response_set_cache_control(http_response response, const char* value);
int response_set_connection(http_response response, const char* value);
void response_set_date(http_response response, time_t value);
int response_set_pragma(http_response response, const char* value);
int response_set_trailer(http_response response, const char* value);
int response_set_transfer_encoding(http_response response, const char* value);
int response_set_upgrade(http_response response, const char* value);
int response_set_via(http_response response, const char* value);
int response_set_warning(http_response response, const char* value);

/* new functions to support cache control stuff */
void response_set_cachecontrol_public(http_response response);
void response_set_cachecontrol_private(http_response response);
void response_set_cachecontrol_no_cache(http_response response);
void response_set_cachecontrol_no_store(http_response response);
void response_set_cachecontrol_no_transform(http_response response);
void response_set_cachecontrol_must_revalidate(http_response response);
void response_set_cachecontrol_proxy_revalidate(http_response response);
void response_set_cachecontrol_max_age(http_response response, int value);
void response_set_cachecontrol_s_maxage(http_response response, int value);


void response_set_accept_ranges(http_response response, int value);
void response_set_age(http_response response, unsigned long value);
int response_set_etag(http_response response, const char* value);
int response_set_location(http_response response, const char* value);
int response_set_proxy_authenticate(http_response response, const char* value);
int response_set_retry_after(http_response response, time_t value);
int response_set_server(http_response response, const char* value);
int response_set_vary(http_response response, const char* value);
int response_set_www_authenticate(http_response response, const char* value);

int response_set_allow(http_response response, const char* value);
int response_set_content_encoding(http_response response, const char* value);
int response_set_content_language(http_response response, const char* value, meta_error e);
int response_set_content_length(http_response response, size_t value);
int response_set_content_location(http_response response, const char* value);
int response_set_content_md5(http_response response, const char* value);
int response_set_content_range(http_response response, const char* value);
int response_set_content_type(http_response response, const char* value);
void response_set_expires(http_response response, time_t value);
void response_set_last_modified(http_response response, time_t value);
int response_add(http_response response, const char* src);
int response_add_char(http_response response, int c);
int response_add_end(http_response response, const char* start, const char* end);
int response_printf(http_response response, size_t size, const char* format, ...)
	__attribute__ ((format(printf, 3, 4)));

void response_set_content_buffer(http_response response, void* src, size_t n);
void response_set_allocated_content_buffer(http_response response, void* src, size_t n);

/* Add embedded, client side javascript, this is highly experimental so beware and enjoy the bugs */
/* messagebox() adds code to display text in a message box */
int response_js_messagebox(http_response response, const char* text);

int response_send_file(http_response response, const char *path, const char* type, meta_error e);

/* Send the complete response to the client */
size_t response_send(http_response r, connection c, meta_error e);
int response_receive(http_response r, connection c, size_t max_content, meta_error e);

/* New stuff 2005-12-14
 * Some formatting functions to ease the generation of HTML.
 * The html module is still not ready so we just add some utility functions here.
 */
int response_br(http_response response);
int response_hr(http_response response);
int response_href(http_response response, const char* ref, const char* text);
int response_p (http_response response, const char* s);
int response_h1(http_response response, const char* s);
int response_h2(http_response response, const char* s);
int response_h3(http_response response, const char* s);
int response_h4(http_response response, const char* s);
int response_h5(http_response response, const char* s);
int response_h6(http_response response, const char* s);
int response_h7(http_response response, const char* s);
int response_h8(http_response response, const char* s);
int response_h9(http_response response, const char* s);
int response_td(http_response response, const char* text);

#define a2p(a, b)	response_add(a, b)

/* attributes */
page_attribute attribute_new(void);
void attribute_free(page_attribute a);

int attribute_set_media_type(page_attribute a, const char* value);
int attribute_set_language(page_attribute a, const char* value);
int attribute_set_charset(page_attribute a, const char* value);
int attribute_set_authorization(page_attribute a, const char* value);
int attribute_set_encoding(page_attribute a, const char* value);

const char*	attribute_get_language(page_attribute a);
const char*	attribute_get_charset(page_attribute a);
const char*	attribute_get_encoding(page_attribute a);
const char*	attribute_get_media_type(page_attribute a);


/* performance counters */
/* These seven functions are wrapper functions for the tcp_server
 * performance counters. */
unsigned long http_server_sum_blocked(http_server s);
unsigned long http_server_sum_discarded(http_server s);
unsigned long http_server_sum_added(http_server s);
unsigned long http_server_sum_poll_intr(http_server p);
unsigned long http_server_sum_poll_again(http_server p);
unsigned long http_server_sum_accept_failed(http_server p);
unsigned long http_server_sum_denied_clients(http_server p);

#ifdef __cplusplus
}
#endif


#endif /* guard */
