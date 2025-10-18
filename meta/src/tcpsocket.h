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

socket_t tcpsocket_accept(int fd, struct sockaddr_storage *addr, socklen_t *addrsize)
    __attribute__((nonnull))
    __attribute__((warn_unused_result))
    __attribute__((malloc));

socket_t tcpsocket_create_client_socket(const char *host, int port)
    __attribute__((malloc))
    __attribute__((nonnull))
    __attribute__((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
