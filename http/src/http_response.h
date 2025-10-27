#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <http_cookie.h>
#include <general_header.h>
#include <entity_header.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_response_tag *http_response;

// HTTP Response
http_response response_new(void) __attribute__((malloc));

void response_free(http_response response);
static inline void response_freev(void *p) { response_free(p); }

void response_recycle(http_response response)
    __attribute__((nonnull));
void response_set_version(http_response response, int version)
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


#ifdef __cplusplus
}
#endif

#endif

