/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
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

tcpsocket tcpsocket_socket(void)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

int tcpsocket_get_fd(tcpsocket p);

tcpsocket tcpsocket_accept(tcpsocket p, struct sockaddr *addr,
    socklen_t *addrsize)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

ssize_t tcpsocket_read(tcpsocket p, char *buf, size_t count,
    int timeout, int retries)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t tcpsocket_poll_for(tcpsocket p, int timeout, short polltype)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t tcpsocket_wait_for_data(tcpsocket p, int timeout)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t tcpsocket_wait_for_writability(tcpsocket p, int timeout)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t tcpsocket_write(tcpsocket p, const char *s, size_t count,
    int timeout, int retries)
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

tcpsocket tcpsocket_create_server_socket(const char *host, int port)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

tcpsocket tcpsocket_create_client_socket(const char *host, int port)
    __attribute__((malloc))
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

status_t tcpsocket_close(tcpsocket p)
    __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
