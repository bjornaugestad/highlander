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

socket_t socket_create_server_socket(int type, const char *host, int port)
    __attribute__((warn_unused_result, nonnull));

socket_t socket_create_client_socket(int type, void *context, const char *host, int port)
    __attribute__((warn_unused_result, nonnull(3)));

socket_t socket_accept(socket_t p, void *context, struct sockaddr *addr,
    socklen_t *addrsize) __attribute__((warn_unused_result, nonnull));

status_t socket_bind(socket_t p, const char *hostname, int port)
    __attribute__((warn_unused_result, nonnull));

status_t socket_listen(socket_t p, int backlog)
    __attribute__((warn_unused_result, nonnull));

status_t socket_close(socket_t p)
    __attribute__((nonnull));

// polltype is POLLIN and friends
status_t socket_poll_for(socket_t p, int timeout, int polltype)
    __attribute__((warn_unused_result, nonnull));

status_t socket_wait_for_data(socket_t p, int timeout)
    __attribute__((warn_unused_result, nonnull));

status_t socket_wait_for_writability(socket_t p, int timeout)
    __attribute__((warn_unused_result, nonnull));

status_t socket_write(socket_t p, const char *s, size_t count, int timeout, int retries)
    __attribute__((warn_unused_result, nonnull));

status_t socket_set_nonblock(socket_t p)
    __attribute__((warn_unused_result, nonnull));

status_t socket_clear_nonblock(socket_t p)
    __attribute__((warn_unused_result, nonnull));

ssize_t  socket_read(socket_t p, char *buf, size_t count, int timeout, int retries)
    __attribute__((warn_unused_result, nonnull));

// boa@20251009: Now that OpenSSL finally has a sane interface, we can merge
// code duplicated in both tcpsocket and sslsocket. We start gently with the two
// set_nonblock/clear_nonblock functions and add two shared functions here.
// Think of gensocket as a C++ parent class for tcpsocket and sslsocket, and
// think of these functions as protected member functions in the parent class.
//
// I'm well aware that we kinda call ourselves here:
// socket_set_nonblock()->tcpsocket_set_nonblock()->gensocket_setnonblock().
// We could of course just have socket_set_nonblock() do the work, but socket_t
// does not hold the fd ATM and who knows if SSL will require more magic in the
// future. And FTR: Old openssl with BIO, locking, thread management and what
// else, was a fucking nightmare!
status_t gensocket_set_nonblock(int fd) __attribute__((warn_unused_result));
status_t gensocket_set_reuse_addr(int fd) __attribute__((warn_unused_result));
status_t gensocket_clear_nonblock(int fd) __attribute__((warn_unused_result));
status_t gensocket_listen(int fd, int backlog) __attribute__((warn_unused_result));

status_t gensocket_bind_inet(int fd, const char *hostname, int port)
 __attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
