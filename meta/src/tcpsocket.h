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

typedef struct tcpsocket_tag *tcpsocket;

tcpsocket tcpsock_socket(void)
    __attribute__((malloc));

status_t tcpsock_listen(tcpsocket p, int backlog)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

tcpsocket tcpsock_accept(tcpsocket p, struct sockaddr *addr,
    socklen_t *addrsize)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((malloc));

ssize_t tcpsock_read(tcpsocket p, char *buf, size_t count,
    int timeout, int retries)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t tcpsock_wait_for_data(tcpsocket p, int timeout)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t tcpsock_wait_for_writability(tcpsocket p, int timeout)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t tcpsock_write(tcpsocket p, const char *s, size_t count,
    int timeout, int retries)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t tcpsock_bind(tcpsocket p, const char *hostname, int port)
    __attribute__((nonnull(1)));

status_t tcpsock_set_nonblock(tcpsocket p)
    __attribute__((nonnull(1)));

status_t tcpsock_clear_nonblock(tcpsocket p)
    __attribute__((nonnull(1)));

tcpsocket tcpsock_create_server_socket(const char *host, int port)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

tcpsocket tcpsock_create_client_socket(const char *host, int port)
    __attribute__((malloc))
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t tcpsock_close(tcpsocket p)
    __attribute__((nonnull(1)));

#ifdef __cplusplus
}
#endif

#endif
