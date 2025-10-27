#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <stddef.h>

#include <general_header.h>
#include <entity_header.h>

#ifndef CHOPPED
#include <http_cookie.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_request_tag *http_request;

const char* request_get_uri(http_request request)
    __attribute__((nonnull, warn_unused_result));

const char* request_get_referer(http_request p)
    __attribute__((nonnull, warn_unused_result));

int request_get_method(http_request request)
    __attribute__((nonnull, warn_unused_result));

int request_get_version(http_request request)
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

#ifndef CHOPPED
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
#endif

/* Returns the HTTP header field if_modified_since or (time_t)-1
 * if the field wasn't set.
 */
time_t request_get_if_modified_since(http_request r)
    __attribute__((nonnull, warn_unused_result));


void request_set_defered_read(http_request req, int flag)
    __attribute__((nonnull));

int request_get_defered_read(http_request req)
    __attribute__((nonnull, warn_unused_result));

http_request request_new(void) __attribute__((malloc, warn_unused_result));

void request_free(http_request p);
static inline void request_freev(void *pv) { request_free(pv); }

status_t request_set_entity(http_request r, void *entity, size_t cb)
    __attribute__((nonnull, warn_unused_result));

void request_set_version(http_request r, int version)
    __attribute__((nonnull));

void request_recycle(http_request r) __attribute__((nonnull));

void request_set_method(http_request r, int method)
    __attribute__((nonnull));

status_t request_set_uri(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_add_param(http_request r, const char *name, const char *value)
    __attribute__((nonnull, warn_unused_result));

status_t request_set_host(http_request r, const char *value)
    __attribute__((nonnull, warn_unused_result));

#ifndef CHOPPED
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

void request_set_max_forwards(http_request r, unsigned int value)
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
#endif

status_t request_send(http_request r, connection c, error e)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t request_receive(http_request r, connection c, size_t max_posted_content, error e)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));


general_header request_get_general_header(http_request r) __attribute__((nonnull, warn_unused_result));
entity_header request_get_entity_header(http_request r) __attribute__((nonnull, warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
