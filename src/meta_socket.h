/*
    libcoremeta - A library of useful C functions and ADT's
    Copyright (C) 2000-2006 B. Augestad, bjorn.augestad@gmail.com 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef META_SOCKET_H
#define META_SOCKET_H

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

/*
 * Creates a new socket.
 * This is a wrapper function for a call to socket().
 * It calls socket with PF_INET as family and SOCK_STREAM
 * as type. Protocol is set to 0.
 */
int sock_socket(int* sock);

/* Calls listen() */
int sock_listen(int sock, int backlog);

/**
 * Tries to read up to count bytes from the socket. 
 * The function will poll for data with a timeout of \e timeout 
 * seconds and retry \e retries times. sock_read() will return 
 * the total number of bytes read in the parameter \e cbReadSum
 * and return 0 if no error occured.
 */
int sock_read(
	int fd,
	char *buf,
	size_t cbMax,
	int timeout,
	int retries,
	size_t* cbReadSum);

/* Waits for data to be available on the socket. */
int wait_for_data(int fd, int timeout);

/*
 * Waits for up to \e timeout seconds to see if it is possible to
 * write to the socket.
 */
int wait_for_writability(int fd, int timeout);

/**
 * Tries to write \e count bytes to the socket, retrying \e retries 
 * times, with a \e timeout seconds sleep (call to poll()).
 *
 * NOTE: sock_write() returns EAGAIN if it was unable to write \e count
 * bytes, even if it was able to write up to \e count - 1 bytes.
 */
int sock_write(int fd, const char* s, size_t count, int timeout, int retries);

/**
 * Binds the socket to an address/port.
 * @return 1 on success, else 0.
 */
int sock_bind(int fd, const char* hostname, int port);

/**
 * Sets the socket to be nonblocking.
 */
int sock_set_nonblock(int fd);

/**
 * Clears the nonblocking flag.
 */
int sock_clear_nonblock(int fd);

/**
 * Creates a server socket.
 * This function calls socket(), sets some socket options,
 * binds the socket to a host/port and calls listen with a 
 * backlog of 100. 
 * 
 * A server socket is ready to accept connections.
 * @return 1 on success, else 0.
 */
int create_server_socket(int *pfd, const char* host, int port);

/*
 * Connects to a host on a port.
 * This function calls gethostbyname() to get host info, and
 * it connects to the h_addr member of struct hostent. h_addr
 * is an alias for h_addrlist[0].
 *
 * @return 1 on success, else 0.
 */
int create_client_socket(int *pfd, const char *host, int port);

/*
 * Closes the socket.
 * sock_close first calls shutdown() with the how parameter set to 
 * SHUT_RDWR. Then it closes the socket using close().
 *
 * @return 1 on success, else 0.
 */
int sock_close(int fd);

#ifdef __cplusplus
}
#endif

#endif

