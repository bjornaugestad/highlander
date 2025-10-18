/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef GENSOCKET_H
#define GENSOCKET_H

#include <sys/socket.h>
#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

// Our socket types, which need to be specified when creating
// our generic object. Since the types requires different
// options, ssl requires certs and shit, we may have to add
// type-specific functions too.
//
#define SOCKTYPE_TCP    1
#define SOCKTYPE_SSL    2

typedef struct gensocket_tag *socket_t;
int socket_get_fd(socket_t this);

socket_t socket_create_server_socket(int type, const char *host, int port)
    __attribute__((warn_unused_result, nonnull));

socket_t socket_create_client_socket(int type, void *context, const char *host, int port)
    __attribute__((warn_unused_result, nonnull(3)));

socket_t socket_accept(socket_t p, void *context, struct sockaddr_storage *addr,
    socklen_t *addrsize) __attribute__((warn_unused_result, nonnull(3,4)));

status_t socket_bind(socket_t p, const char *hostname, int port)
    __attribute__((warn_unused_result, nonnull));

status_t socket_close(socket_t p)
    __attribute__((nonnull));

// polltype is POLLIN and friends
status_t socket_poll_for(int fd, int timeout, int polltype)
    __attribute__((warn_unused_result));

status_t socket_wait_for_data(socket_t p, int timeout)
    __attribute__((warn_unused_result, nonnull));

status_t socket_write(socket_t p, const char *s, size_t count, int timeout, int retries)
    __attribute__((warn_unused_result, nonnull));

ssize_t  socket_read(socket_t p, char *buf, size_t count, int timeout, int retries)
    __attribute__((warn_unused_result, nonnull));

status_t socket_set_nonblock(int fd) __attribute__((warn_unused_result));
status_t socket_set_reuse_addr(int fd) __attribute__((warn_unused_result));
status_t socket_clear_nonblock(int fd) __attribute__((warn_unused_result));
status_t socket_listen(int fd, int backlog) __attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
