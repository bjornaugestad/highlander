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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file meta_socket.h
 * @brief Uitility functions for sockets
 *
 * NOTE: This file is just a collection of utility functions manipulating
 * sockets. It's not a proper adt. Even a bug or two exist, and the
 * return codes are very inconsistent. All of this means that sock.[ch]
 * is up for a redesign/rewrite.
 * (BTW: the known bug is gethostname() instead of gethostname_r() in
 * reentrant programs).
 */


typedef struct meta_socket_tag *meta_socket;

/*
 * Creates a new socket.
 * This is a wrapper function for a call to socket().
 * If unix_socket is !0, then a UNIX socket is created, eles
 * an INET socket is created.
 */
meta_socket sock_socket(int unix_socket);

/* Calls listen() */
int sock_listen(meta_socket p, int backlog);
meta_socket sock_accept(meta_socket p, struct sockaddr *addr, socklen_t *addrsize);

/**
 * Tries to read up to count bytes from the socket.
 * The function will poll for data with a timeout of \e timeout
 * seconds and retry \e retries times. sock_read() will return
 * the total number of bytes read in the parameter \e cbReadSum
 * and return 0 if no error occured.
 */
int sock_read(
	meta_socket p,
	char *buf,
	size_t cbMax,
	int timeout,
	int retries,
	size_t* cbReadSum);

/* Waits for data to be available on the socket. */
int wait_for_data(meta_socket p, int timeout);

/*
 * Waits for up to \e timeout seconds to see if it is possible to
 * write to the socket.
 */
int wait_for_writability(meta_socket p, int timeout);

/**
 * Tries to write \e count bytes to the socket, retrying \e retries
 * times, with a \e timeout seconds sleep (call to poll()).
 *
 * NOTE: sock_write() returns EAGAIN if it was unable to write \e count
 * bytes, even if it was able to write up to \e count - 1 bytes.
 */
int sock_write(meta_socket p, const char* s, size_t count, int timeout, int retries);

/**
 * Binds the socket to an address/port.
 * @return 1 on success, else 0.
 */
int sock_bind(meta_socket p, const char* hostname, int port);

/**
 * Sets the socket to be nonblocking.
 */
int sock_set_nonblock(meta_socket p);

/**
 * Clears the nonblocking flag.
 */
int sock_clear_nonblock(meta_socket p);

/**
 * Creates a server socket.
 * This function calls socket(), sets some socket options,
 * binds the socket to a host/port and calls listen with a
 * backlog of 100.
 *
 * If unix_socket is 1, then the function will create a Unix socket,
 * else it will create a regular AF_INET socket.
 *
 * A server socket is ready to accept connections.
 * @return 1 on success, else 0.
 */
meta_socket create_server_socket(int unix_socket, const char* host, int port);

/*
 * Connects to a host on a port.
 * This function calls gethostbyname() to get host info, and
 * it connects to the h_addr member of struct hostent. h_addr
 * is an alias for h_addrlist[0].
 *
 * @return 1 on success, else 0.
 */
meta_socket create_client_socket(const char *host, int port);

/*
 * Closes the socket.
 * sock_close first calls shutdown() with the how parameter set to
 * SHUT_RDWR. Then it closes the socket using close().
 *
 * @return 1 on success, else 0.
 */
int sock_close(meta_socket p);

#ifdef __cplusplus
}
#endif

#endif

