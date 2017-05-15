/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_SSL_H
#define META_SSL_H

#include <sys/socket.h>
#include <meta_membuf.h>
#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct meta_ssl_tag *meta_ssl;

meta_ssl ssl_socket(void);
status_t ssl_listen(meta_ssl p, int backlog);
meta_ssl ssl_accept(meta_ssl p, struct sockaddr *addr, socklen_t *addrsize);
ssize_t  ssl_read(meta_ssl p, char *buf, size_t count, int timeout, int retries);
status_t ssl_wait_for_data(meta_ssl p, int timeout);
status_t ssl_wait_for_writability(meta_ssl p, int timeout);
status_t ssl_write(meta_ssl p, const char *s, size_t count, int timeout, int retries);
status_t ssl_bind(meta_ssl p, const char *hostname, int port);
status_t ssl_set_nonblock(meta_ssl p);
status_t ssl_clear_nonblock(meta_ssl p);
meta_ssl ssl_create_server_socket(const char *host, int port);
meta_ssl ssl_create_client_socket(const char *host, int port);
status_t ssl_close(meta_ssl p);

#ifdef __cplusplus
}
#endif

#endif
