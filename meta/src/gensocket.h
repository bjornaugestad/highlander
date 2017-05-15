/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef GENSOCKET_H
#define GENSOCKET_H

#include <sys/socket.h>
#include <meta_membuf.h>
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

typedef struct gensocket_tag *sock;

sock sock_create_server_socket(int type, const char *host, int port);
sock sock_create_client_socket(int type, const char *host, int port);
sock sock_socket(int type);
sock sock_accept(sock p, struct sockaddr *addr, socklen_t *addrsize);

status_t sock_bind(sock p, const char *hostname, int port);
status_t sock_listen(sock p, int backlog);
status_t sock_close(sock p);

status_t sock_wait_for_data(sock p, int timeout);
status_t sock_wait_for_writability(sock p, int timeout);
status_t sock_write(sock p, const char *s, size_t count, int timeout, int retries);
status_t sock_set_nonblock(sock p);
status_t sock_clear_nonblock(sock p);
ssize_t  sock_read(sock p, char *buf, size_t count, int timeout, int retries);

#ifdef __cplusplus
}
#endif

#endif
