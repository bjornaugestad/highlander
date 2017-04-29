/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_SOCKET_H
#define META_SOCKET_H

#include <sys/socket.h>
#include <meta_membuf.h>
#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct meta_socket_tag *meta_socket;

meta_socket sock_socket(int unix_socket)
    __attribute__((malloc));

status_t sock_listen(meta_socket p, int backlog)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

meta_socket sock_accept(meta_socket p, struct sockaddr *addr,
    socklen_t *addrsize)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((malloc));

ssize_t sock_read(meta_socket p, char *buf, size_t count,
    int timeout, int retries)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t wait_for_data(meta_socket p, int timeout)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t wait_for_writability(meta_socket p, int timeout)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t sock_write(meta_socket p, const char *s, size_t count,
    int timeout, int retries)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t sock_bind(meta_socket p, const char *hostname, int port)
    __attribute__((nonnull(1)));

status_t sock_set_nonblock(meta_socket p)
    __attribute__((nonnull(1)));

status_t sock_clear_nonblock(meta_socket p)
    __attribute__((nonnull(1)));

meta_socket create_server_socket(int unix_socket, const char *host, int port)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

meta_socket create_client_socket(const char *host, int port)
    __attribute__((malloc))
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t sock_close(meta_socket p)
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
