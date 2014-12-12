/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
	__attribute__((warn_unused_result));

meta_socket sock_accept(meta_socket p, struct sockaddr *addr,
	socklen_t *addrsize)
	__attribute__((malloc));

ssize_t sock_read(meta_socket p, char *buf, size_t count,
	int timeout, int retries)
	__attribute__((warn_unused_result));

status_t wait_for_data(meta_socket p, int timeout)
	__attribute__((warn_unused_result));

status_t wait_for_writability(meta_socket p, int timeout)
	__attribute__((warn_unused_result));

status_t sock_write(meta_socket p, const char *s, size_t count,
	int timeout, int retries)
	__attribute__((warn_unused_result));

status_t sock_bind(meta_socket p, const char *hostname, int port);

status_t sock_set_nonblock(meta_socket p);
status_t sock_clear_nonblock(meta_socket p);

meta_socket create_server_socket(int unix_socket, const char *host, int port)
	__attribute__((malloc))
	__attribute__((warn_unused_result));

meta_socket create_client_socket(const char *host, int port)
	__attribute__((malloc))
	__attribute__((warn_unused_result));

status_t sock_close(meta_socket p);

#ifdef __cplusplus
}
#endif

#endif
