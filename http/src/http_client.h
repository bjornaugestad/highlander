#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <http_request.h>
#include <http_response.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_client_tag *http_client;

// new stuff 20141231: We need a client ADT for better testing of the server
http_client http_client_new(int socktype) __attribute__((warn_unused_result));
void http_client_free(http_client p);
status_t http_client_connect(http_client this, const char *host, uint16_t port)
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

unsigned http_client_get_timeout_write(http_client p)
    __attribute__((nonnull, warn_unused_result));

unsigned http_client_get_timeout_read(http_client p)
    __attribute__((nonnull, warn_unused_result));


void http_client_set_timeout_write(http_client p, unsigned millisec) __attribute__((nonnull));
void http_client_set_timeout_read(http_client p, unsigned millisec)  __attribute__((nonnull));
void http_client_set_retries_read(http_client p, unsigned count)     __attribute__((nonnull));
void http_client_set_retries_write(http_client p, unsigned count)    __attribute__((nonnull));


#ifdef __cplusplus
}
#endif

#endif




