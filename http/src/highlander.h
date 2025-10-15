/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */


#ifndef HIGHLANDER_H
#define HIGHLANDER_H

#include <stdbool.h>
#include <time.h>

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
typedef struct http_client_tag*		http_client;
typedef struct http_request_tag*	http_request;
typedef struct http_response_tag*	http_response;
typedef enum http_method			http_method;
typedef enum http_version			http_version;
typedef struct cookie_tag*			cookie;
typedef struct dynamic_page_tag*	dynamic_page;
typedef struct page_attribute_tag*	page_attribute;
typedef struct general_header_tag*	general_header;
typedef struct entity_header_tag*	entity_header;

typedef int (*handlerfn)(http_request, http_response);

// new stuff 20141231: We need a client ADT for better testing of the server
http_client http_client_new(int socktype) __attribute__((warn_unused_result));
void http_client_free(http_client p);
status_t http_client_connect(http_client this, const char *host, int port)
    __attribute__((warn_unused_result));

status_t http_client_get(http_client this, const char *host, const char *uri, error e)
    __attribute__((warn_unused_result));

status_t http_client_post(http_client this, const char *host, const char *uri, error e)
    __attribute__((warn_unused_result));

int http_client_http_status(http_client this)
    __attribute__((warn_unused_result));

http_response http_client_response(http_client this)
    __attribute__((warn_unused_result));

status_t http_client_disconnect(http_client this);

int http_client_get_timeout_write(http_client p)
    __attribute__((nonnull, warn_unused_result));

int http_client_get_timeout_read(http_client p)
    __attribute__((nonnull, warn_unused_result));

int http_client_get_timeout_read(http_client p)
    __attribute__((nonnull, warn_unused_result));


void http_client_set_timeout_write(http_client p, int millisec) __attribute__((nonnull));
void http_client_set_timeout_read(http_client p, int millisec)  __attribute__((nonnull));
void http_client_set_retries_read(http_client p, int count)     __attribute__((nonnull));
void http_client_set_retries_write(http_client p, int count)    __attribute__((nonnull));


http_server http_server_new(int socktype) __attribute__((warn_unused_result));
void http_server_free(http_server s);

status_t http_server_configure(http_server s, process p, const char *filename)
    __attribute__((nonnull(1, 3), warn_unused_result));

status_t http_server_start_via_process(process p, http_server s)
    __attribute__((nonnull, warn_unused_result));

void http_server_set_can_read_files(http_server s, int val)
    __attribute__((nonnull));

int http_server_can_read_files(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_alloc(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_get_root_resources(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_free_root_resources(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_start(http_server srv)
    __attribute__((nonnull, warn_unused_result));

int http_server_shutting_down(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_shutdown(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_add_page(http_server s, const char *uri, handlerfn pf, page_attribute attr)
    __attribute__((nonnull(1,2,3)))
    __attribute__((warn_unused_result));

void http_server_trace(http_server s, int level) __attribute__((nonnull)); 
void http_server_set_defered_read(http_server s, int flag) __attribute__((nonnull));

int http_server_get_defered_read(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_set_documentroot(http_server s, const char *docroot)
    __attribute__((nonnull, warn_unused_result));

void http_server_set_post_limit(http_server s, size_t cb)
    __attribute__((nonnull));

size_t http_server_get_post_limit(http_server s)
    __attribute__((nonnull, warn_unused_result));

const char* http_server_get_documentroot(http_server s)
    __attribute__((nonnull, warn_unused_result));

void http_server_set_default_page_handler(http_server s, handlerfn pf)
    __attribute__((nonnull));

status_t http_server_set_default_page_attributes(http_server s, page_attribute a)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_set_host(http_server s, const char *name)
    __attribute__((nonnull, warn_unused_result));

void http_server_set_port(http_server s, int n)
    __attribute__((nonnull));

int http_server_get_timeout_write(http_server s)    __attribute__((nonnull, warn_unused_result));
int http_server_get_timeout_read(http_server srv)   __attribute__((nonnull, warn_unused_result));
int http_server_get_timeout_accept(http_server srv) __attribute__((nonnull, warn_unused_result));
int http_server_get_timeout_read(http_server s)     __attribute__((nonnull, warn_unused_result));

void http_server_set_timeout_write(http_server s, int seconds)  __attribute__((nonnull)); 
void http_server_set_timeout_read(http_server s, int seconds)   __attribute__((nonnull)); 
void http_server_set_timeout_accept(http_server s, int seconds) __attribute__((nonnull)); 
void http_server_set_retries_read(http_server s, int seconds)   __attribute__((nonnull)); 
void http_server_set_retries_write(http_server s, int seconds)  __attribute__((nonnull));


void http_server_set_worker_threads(http_server s, size_t n) __attribute__((nonnull));
void http_server_set_queue_size(http_server s, size_t n)     __attribute__((nonnull));
void http_server_set_max_pages(http_server s, size_t n)      __attribute__((nonnull));
void http_server_set_block_when_full(http_server s, int n)   __attribute__((nonnull));

status_t http_server_set_logfile(http_server s, const char *logfile)
    __attribute__((nonnull, warn_unused_result));

void http_server_set_logrotate(http_server s, int logrotate)
    __attribute__((nonnull));

int http_server_get_port(http_server s)
    __attribute__((nonnull, warn_unused_result));

size_t http_server_get_worker_threads(http_server s)
    __attribute__((nonnull, warn_unused_result));

size_t http_server_get_queue_size(http_server s)
    __attribute__((nonnull, warn_unused_result));

int   http_server_get_block_when_full(http_server s)
    __attribute__((nonnull, warn_unused_result));

size_t http_server_get_max_pages(http_server s)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_set_server_cert_chain_file(http_server p, const char *path)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_set_private_key(http_server p, const char *path)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_set_ca_directory(http_server p, const char *path)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_uri(http_request request)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_referer(http_request p)
    __attribute__((nonnull, warn_unused_result));

http_method request_get_method(http_request request)
    __attribute__((nonnull, warn_unused_result));

http_version request_get_version(http_request request)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_host(http_request request)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_content(http_request request)
    __attribute__((nonnull, warn_unused_result));

size_t request_get_content_length(http_request request)
    __attribute__((nonnull, warn_unused_result));


/* request arguments */
int	request_get_parameter_count(http_request request)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_parameter_name (http_request request, size_t i)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_parameter_value(http_request request, const char *name)
    __attribute__((nonnull, warn_unused_result));

/* Forms */
size_t request_get_field_count(http_request request)
    __attribute__((nonnull, warn_unused_result));

size_t request_get_field_namelen(http_request request, size_t i)
    __attribute__((nonnull, warn_unused_result));

size_t request_get_field_valuelen(http_request request, size_t i)
    __attribute__((nonnull, warn_unused_result));

status_t request_get_field_name(http_request request, size_t i, char *s, size_t cb)
    __attribute__((nonnull, warn_unused_result));

status_t request_get_field_value(http_request request, size_t i, char *s, size_t cb)
    __attribute__((nonnull, warn_unused_result));

status_t request_get_field_value_by_name(http_request request,
    const char *name, char *value, size_t cb)
    __attribute__((nonnull, warn_unused_result));

/* request cookies */
size_t request_get_cookie_count(http_request request)
    __attribute__((nonnull, warn_unused_result));

cookie request_get_cookie(http_request request, size_t i)
    __attribute__((nonnull, warn_unused_result));

int request_content_type_is(http_request request, const char *val)
    __attribute__((nonnull, warn_unused_result));

bool request_accepts_media_type(http_request request, const char *val)
    __attribute__((nonnull, warn_unused_result));

bool request_accepts_language(http_request request, const char *lang)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_content_type(http_request request)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_user_agent(http_request request)
    __attribute__((nonnull, warn_unused_result));

/* Returns the HTTP header field if_modified_since or (time_t)-1
 * if the field wasn't set.
 */
time_t request_get_if_modified_since(http_request r)
    __attribute__((nonnull, warn_unused_result));

/* Cookies */
cookie cookie_new(void) __attribute__((malloc));

status_t cookie_set_name(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_value(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_comment(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_domain(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_path(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 

void cookie_set_max_age(cookie c, int value) __attribute__((nonnull)); 
void cookie_set_version(cookie c, int value) __attribute__((nonnull)); 
void cookie_set_secure(cookie c, int value) __attribute__((nonnull)); 
const char*	cookie_get_name(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_value(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_comment(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_domain(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_path(cookie c)
    __attribute__((nonnull, warn_unused_result));

int cookie_get_version(cookie c)
    __attribute__((nonnull, warn_unused_result));

int cookie_get_secure(cookie c)
    __attribute__((nonnull, warn_unused_result));

int cookie_get_max_age(cookie c)
    __attribute__((nonnull, warn_unused_result));

connection request_get_connection(http_request req)
    __attribute__((nonnull, warn_unused_result));

// HTTP Request
void request_set_defered_read(http_request req, int flag)
    __attribute__((nonnull));

int request_get_defered_read(http_request req)
    __attribute__((nonnull, warn_unused_result));

http_request request_new(void) __attribute__((malloc, warn_unused_result));

void request_free(http_request p);

status_t request_set_entity(http_request r, void *entity, size_t cb)
    __attribute__((nonnull, warn_unused_result));

void request_set_version(http_request r, http_version version)
    __attribute__((nonnull));

void request_recycle(http_request r) __attribute__((nonnull));

status_t request_add_param(http_request r, const char *name, const char *value)
    __attribute__((nonnull, warn_unused_result));

void request_set_method(http_request r, http_method method)
    __attribute__((nonnull));

status_t request_set_uri(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_mime_version(http_request request, int major, int minor, error e)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_accept_ranges(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_age(http_request r, unsigned long value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_etag(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_location(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_proxy_authenticate(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_retry_after(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_server(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_vary(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_www_authenticate(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_accept(http_request r, const char *value, error e)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_accept_charset(http_request r, const char *value, error e)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_accept_encoding(http_request r, const char *value, error e)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_accept_language(http_request r, const char *value, error e)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_authorization(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_expect(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_from(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_host(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_if_match(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

void request_set_if_modified_since(http_request r, time_t value)
    __attribute__((nonnull));

status_t request_set_if_none_match(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_if_range(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_if_unmodified_since(http_request r, time_t value)
    __attribute__((nonnull, warn_unused_result));

void request_set_max_forwards(http_request r, unsigned long value)
    __attribute__((nonnull));

status_t request_set_proxy_authorization(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_range(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_referer(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_te(http_request r, const char *value, error e)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_user_agent(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_add_cookie(http_request r, cookie c)
    __attribute__((nonnull, warn_unused_result));

status_t request_send(http_request r, connection c, error e)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t request_receive(http_request r, connection c, size_t max_posted_content, error e)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));


general_header request_get_general_header(http_request r) __attribute__((nonnull, warn_unused_result));
entity_header request_get_entity_header(http_request r) __attribute__((nonnull, warn_unused_result));

// HTTP Response
http_response response_new(void) __attribute__((malloc));

void response_free(http_response response);
void response_recycle(http_response response)
    __attribute__((nonnull));
void response_set_version(http_response response, http_version version)
    __attribute__((nonnull));
void response_set_status(http_response response, int status)
    __attribute__((nonnull));
int response_get_status(http_response response)
    __attribute__((nonnull, warn_unused_result));

general_header response_get_general_header(http_response r) 
    __attribute__((nonnull, warn_unused_result));
entity_header response_get_entity_header(http_response r) 
    __attribute__((nonnull, warn_unused_result));

/* Note that file argument *MUST* be a FILE* */
void response_dump(http_response r, void *file)         __attribute__((nonnull));
void general_header_dump(general_header gh, void *file) __attribute__((nonnull));
void entity_header_dump(entity_header gh, void *file)   __attribute__((nonnull));

const char* response_get_connection(http_response response) __attribute__((nonnull, warn_unused_result));
const char*	response_get_entity(http_response p)            __attribute__((nonnull, warn_unused_result));
size_t      response_get_content_length(http_response p)    __attribute__((nonnull, warn_unused_result));

status_t response_set_cookie(http_response response, cookie c)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_cache_control(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_connection(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

void response_set_date(http_response response, time_t value)
    __attribute__((nonnull));

status_t response_set_pragma(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_trailer(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_transfer_encoding(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_upgrade(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_via(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_warning(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

/* new functions to support cache control stuff */
void response_set_cachecontrol_public(http_response response) __attribute__((nonnull)); 
void response_set_cachecontrol_private(http_response response) __attribute__((nonnull)); 
void response_set_cachecontrol_no_cache(http_response response) __attribute__((nonnull)); 
void response_set_cachecontrol_no_store(http_response response) __attribute__((nonnull)); 
void response_set_cachecontrol_no_transform(http_response response) __attribute__((nonnull)); 
void response_set_cachecontrol_must_revalidate(http_response response) __attribute__((nonnull)); 
void response_set_cachecontrol_proxy_revalidate(http_response response) __attribute__((nonnull)); 
void response_set_cachecontrol_max_age(http_response response, int value) __attribute__((nonnull)); 
void response_set_cachecontrol_s_maxage(http_response response, int value) __attribute__((nonnull)); 

void response_set_accept_ranges(http_response response, int value) __attribute__((nonnull)); 
void response_set_age(http_response response, unsigned long value) __attribute__((nonnull));

status_t response_set_etag(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_location(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_proxy_authenticate(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

void response_set_retry_after(http_response response, time_t value)
    __attribute__((nonnull));

status_t response_set_server(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_vary(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_www_authenticate(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_allow(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_content_encoding(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_content_language(http_response response, const char *value, error e)
    __attribute__((nonnull, warn_unused_result));

void response_set_content_length(http_response response, size_t value) __attribute__((nonnull));

status_t response_set_content_location(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_content_md5(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_content_range(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t response_set_content_type(http_response response, const char *value)
    __attribute__((nonnull, warn_unused_result));

void response_set_expires(http_response response, time_t value)
    __attribute__((nonnull));

void response_set_last_modified(http_response response, time_t value)
    __attribute__((nonnull));

status_t response_add(http_response response, const char *src)
    __attribute__((nonnull, warn_unused_result));

status_t response_add_char(http_response response, int c)
    __attribute__((nonnull, warn_unused_result));

status_t response_add_end(http_response response, const char *start, const char *end)
    __attribute__((nonnull, warn_unused_result));

status_t response_printf(http_response response, const char *format, ...)
    __attribute__ ((format(printf, 2, 3)))
    __attribute__((nonnull, warn_unused_result));

void response_set_content_buffer(http_response response, void *src, size_t n)
    __attribute__((nonnull));

void response_set_allocated_content_buffer(http_response response, void *src, size_t n)
    __attribute__((nonnull));


/* Add embedded, client side javascript, this is highly experimental
 * so beware and enjoy the bugs */
/* messagebox() adds code to display text in a message box */
status_t response_js_messagebox(http_response response, const char *text)
    __attribute__((nonnull, warn_unused_result));


status_t response_send_file(http_response response, const char *path, const char *type, error e)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));


/* Send the complete response to the client */
status_t response_send(http_response r, connection c, error e, size_t *pcb)
    __attribute__((nonnull(1, 2, 4)))
    __attribute__((warn_unused_result));

status_t response_receive(http_response r, connection c, size_t max_content, error e)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

/* New stuff 2005-12-14
 * Some formatting functions to ease the generation of HTML.
 * The html module is still not ready so we just add some utility functions here.
 */
status_t response_br(http_response response) __attribute__((nonnull, warn_unused_result)); 
status_t response_hr(http_response response) __attribute__((nonnull, warn_unused_result)); 
status_t response_href(http_response response, const char *ref, const char *text) __attribute__((nonnull, warn_unused_result)); 
status_t response_p(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h1(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h2(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h3(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h4(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h5(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h6(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h7(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h8(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_h9(http_response response, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t response_td(http_response response, const char *text) __attribute__((nonnull, warn_unused_result)); 

/* attributes */
page_attribute attribute_new(void) __attribute__((malloc, warn_unused_result));
void attribute_free(page_attribute a);

status_t attribute_set_media_type   (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_language     (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_charset      (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_authorization(page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_encoding     (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 

const char*	attribute_get_language  (page_attribute a) __attribute__((nonnull, warn_unused_result)); 
const char*	attribute_get_charset   (page_attribute a) __attribute__((nonnull, warn_unused_result)); 
const char*	attribute_get_encoding  (page_attribute a) __attribute__((nonnull, warn_unused_result)); 
const char*	attribute_get_media_type(page_attribute a) __attribute__((nonnull, warn_unused_result));

/* performance counters */
/* These seven functions are wrapper functions for the tcp_server
 * performance counters. */
unsigned long http_server_sum_blocked(http_server s)        __attribute__((nonnull, warn_unused_result));
unsigned long http_server_sum_discarded(http_server s)      __attribute__((nonnull, warn_unused_result));
unsigned long http_server_sum_added(http_server s)          __attribute__((nonnull, warn_unused_result));
unsigned long http_server_sum_poll_intr(http_server p)      __attribute__((nonnull, warn_unused_result));
unsigned long http_server_sum_poll_again(http_server p)     __attribute__((nonnull, warn_unused_result));
unsigned long http_server_sum_accept_failed(http_server p)  __attribute__((nonnull, warn_unused_result));
unsigned long http_server_sum_denied_clients(http_server p) __attribute__((nonnull, warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
