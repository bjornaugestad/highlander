/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef SSLSOCKET_H
#define SSLSOCKET_H

#include <sys/socket.h> // for the types

#include <meta_common.h>
#include <gensocket.h>

#ifdef __cplusplus
extern "C" {
#endif


bool sslsocket_pending(socket_t this);

socket_t sslsocket_accept(socket_t p, void *ssl_ctx, 
    struct sockaddr_storage *addr, socklen_t *addrsize)
        __attribute__((warn_unused_result));

status_t sslsocket_clear_nonblock(socket_t p)
    __attribute__((warn_unused_result));

// context should be a valid pointer to an SSL_CTX object.
socket_t sslsocket_create_client_socket(void *context, const char *host, int port)
    __attribute__((warn_unused_result));

status_t sslsocket_close(socket_t p);

status_t sslsocket_set_rootcert(socket_t p, const char *path)
    __attribute__((warn_unused_result));

status_t sslsocket_set_private_key(socket_t p, const char *path)
    __attribute__((warn_unused_result));

status_t sslsocket_set_ciphers(socket_t p, const char *ciphers)
    __attribute__((warn_unused_result));

status_t sslsocket_set_ca_directory(socket_t p, const char *path)
    __attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
