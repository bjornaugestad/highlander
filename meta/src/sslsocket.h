/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef SSLSOCKET_H
#define SSLSOCKET_H

#include <sys/socket.h> // for the types

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sslsocket_tag *sslsocket;
int sslsocket_get_fd(sslsocket p);

struct addrinfo;
sslsocket sslsocket_socket(struct addrinfo *ai)
    __attribute__((warn_unused_result));

status_t sslsocket_listen(sslsocket p, int backlog)
    __attribute__((warn_unused_result));

sslsocket sslsocket_accept(sslsocket p, void *ssl_ctx, 
    struct sockaddr *addr, socklen_t *addrsize)
        __attribute__((warn_unused_result));

ssize_t  sslsocket_read(sslsocket p, char *buf, size_t count, int timeout, int retries)
    __attribute__((warn_unused_result));

status_t sslsocket_wait_for_data(sslsocket p, int timeout)
    __attribute__((warn_unused_result));

status_t sslsocket_write(sslsocket p, const char *s, size_t count, int timeout, int retries)
    __attribute__((warn_unused_result));

status_t sslsocket_clear_nonblock(sslsocket p)
    __attribute__((warn_unused_result));

sslsocket sslsocket_create_server_socket(const char *host, int port)
    __attribute__((warn_unused_result));

// context should be a valid pointer to an SSL_CTX object.
sslsocket sslsocket_create_client_socket(void *context, const char *host, int port)
    __attribute__((warn_unused_result));

status_t sslsocket_close(sslsocket p);

status_t sslsocket_set_rootcert(sslsocket p, const char *path)
    __attribute__((warn_unused_result));

status_t sslsocket_set_private_key(sslsocket p, const char *path)
    __attribute__((warn_unused_result));

status_t sslsocket_set_ciphers(sslsocket p, const char *ciphers)
    __attribute__((warn_unused_result));

status_t sslsocket_set_ca_directory(sslsocket p, const char *path)
    __attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
