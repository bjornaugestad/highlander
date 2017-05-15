/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef SSLSOCKET_H
#define SSLSOCKET_H

#include <sys/socket.h>
#include <meta_membuf.h>
#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sslsocket_tag *sslsocket;

sslsocket sslsocket_socket(void);
status_t sslsocket_listen(sslsocket p, int backlog);
sslsocket sslsocket_accept(sslsocket p, struct sockaddr *addr, socklen_t *addrsize);
ssize_t  sslsocket_read(sslsocket p, char *buf, size_t count, int timeout, int retries);
status_t sslsocket_wait_for_data(sslsocket p, int timeout);
status_t sslsocket_wait_for_writability(sslsocket p, int timeout);
status_t sslsocket_write(sslsocket p, const char *s, size_t count, int timeout, int retries);
status_t sslsocket_bind(sslsocket p, const char *hostname, int port);
status_t sslsocket_set_nonblock(sslsocket p);
status_t sslsocket_clear_nonblock(sslsocket p);
sslsocket sslsocket_create_server_socket(const char *host, int port);
sslsocket sslsocket_create_client_socket(const char *host, int port);
status_t sslsocket_close(sslsocket p);

#ifdef __cplusplus
}
#endif

#endif