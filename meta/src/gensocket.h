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

typedef struct gensocket_tag *socket_t;

socket_t socket_create_server_socket(int type, const char *host, int port);
socket_t socket_create_client_socket(int type, const char *host, int port);
socket_t socket_socket(int type);
socket_t socket_accept(socket_t p, struct sockaddr *addr, socklen_t *addrsize);

status_t socket_bind(socket_t p, const char *hostname, int port);
status_t socket_listen(socket_t p, int backlog);
status_t socket_close(socket_t p);

// polltype is POLLIN and friends
status_t socket_poll_for(socket_t p, int timeout, int polltype);
status_t socket_wait_for_data(socket_t p, int timeout);
status_t socket_wait_for_writability(socket_t p, int timeout);
status_t socket_write(socket_t p, const char *s, size_t count, int timeout, int retries);
status_t socket_set_nonblock(socket_t p);
status_t socket_clear_nonblock(socket_t p);
ssize_t  socket_read(socket_t p, char *buf, size_t count, int timeout, int retries);
#ifdef __cplusplus
}
#endif

#endif
