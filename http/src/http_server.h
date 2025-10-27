#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <meta_process.h>
#include <http_request.h>
#include <http_response.h>

#include <page_attribute.h>
#include <dynamic_page.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_server_tag *http_server;

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

status_t http_server_add_page(http_server s, const char *uri, int (*pfn)(http_request, http_response), page_attribute attr)
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

void http_server_set_default_page_handler(http_server s, int (*pfn)(http_request, http_response))
    __attribute__((nonnull));

status_t http_server_set_default_page_attributes(http_server s, page_attribute a)
    __attribute__((nonnull, warn_unused_result));

status_t http_server_set_host(http_server s, const char *name)
    __attribute__((nonnull, warn_unused_result));

void http_server_set_port(http_server s, uint16_t n) __attribute__((nonnull));

unsigned http_server_get_timeout_write(http_server s)    __attribute__((nonnull, warn_unused_result));
unsigned http_server_get_timeout_read(http_server srv)   __attribute__((nonnull, warn_unused_result));
unsigned http_server_get_timeout_accept(http_server srv) __attribute__((nonnull, warn_unused_result));
unsigned http_server_get_timeout_read(http_server s)     __attribute__((nonnull, warn_unused_result));

void http_server_set_timeout_write(http_server s, unsigned seconds)  __attribute__((nonnull)); 
void http_server_set_timeout_read(http_server s, unsigned seconds)   __attribute__((nonnull)); 
void http_server_set_timeout_accept(http_server s, unsigned seconds) __attribute__((nonnull)); 
void http_server_set_retries_read(http_server s, unsigned seconds)   __attribute__((nonnull)); 
void http_server_set_retries_write(http_server s, unsigned seconds)  __attribute__((nonnull));


void http_server_set_worker_threads(http_server s, size_t n) __attribute__((nonnull));
void http_server_set_queue_size(http_server s, size_t n)     __attribute__((nonnull));
void http_server_set_max_pages(http_server s, size_t n)      __attribute__((nonnull));
void http_server_set_block_when_full(http_server s, int n)   __attribute__((nonnull));

status_t http_server_set_logfile(http_server s, const char *logfile)
    __attribute__((nonnull, warn_unused_result));

void http_server_set_logrotate(http_server s, int logrotate)
    __attribute__((nonnull));

uint16_t http_server_get_port(http_server s)
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


dynamic_page http_server_lookup(http_server srv, http_request request);
http_request http_server_get_request(http_server srv);
http_response http_server_get_response(http_server srv);
page_attribute http_server_get_default_attributes(http_server srv);
void http_server_recycle_request(http_server srv, http_request request);
void http_server_recycle_response(http_server srv, http_response response);
void http_server_add_logentry(http_server srv, connection conn, http_request req, int sc, size_t cb);

/* Handles the default page */
bool http_server_has_default_page_handler(http_server s);

status_t http_server_run_default_page_handler(http_server s,
    http_request request, http_response response, error e) __attribute__((nonnull, warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif



