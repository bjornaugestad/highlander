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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif


#include <meta_socket.h>

struct meta_socket_tag {
	int fd;
	int unix_socket;
};


/*
 * Sets the socket options we want on the main socket
 * Suitable for server sockets only.
 */
static int sock_set_reuseaddr(meta_socket p)
{
	int optval;
	socklen_t optlen;

	assert(p != NULL);
	assert(p->fd >= 0);

	optval = 1;
	optlen = (socklen_t)sizeof(optval);
	if (setsockopt(p->fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) == -1)
		return 0;

	return 1;
}


/*
 * This is a local helper function. It polls for some kind of event,
 * which normally is POLLIN or POLLOUT.
 * The function returns 1 if the event has occured, and 0 if an
 * error occured. It set errno to EAGAIN if a timeout occured, and
 * it maps POLLHUP and POLLERR to EPIPE, and POLLNVAL to EINVAL.
 */
static int sock_poll_for(meta_socket p, int timeout, short poll_for)
{
	struct pollfd pfd;
	int rc;
	int status = 0;

	assert(p != NULL);
	assert(p->fd >= 0);
	assert(poll_for == POLLIN || poll_for == POLLOUT);
	assert(timeout >= 0);

	pfd.fd = p->fd;
	pfd.events = poll_for;

	/* NOTE: poll is XPG4, not POSIX */
	rc = poll(&pfd, 1, timeout);
	if (rc == 1) {
		/* We have info in pfd */
		if (pfd.revents & POLLHUP) {
			errno = EPIPE;
		}
		else if (pfd.revents & POLLERR) {
			errno = EPIPE;
		}
		else if (pfd.revents & POLLNVAL) {
			errno = EINVAL;
		}
		else if ((pfd.revents & poll_for)  == poll_for) {
			status = 1;
		}
	}
	else if (rc == 0) {
		errno = EAGAIN;
	}
	else if (rc == -1) {
	}

	return status;
}

int wait_for_writability(meta_socket p, int timeout)
{
	assert(p != NULL);

	return sock_poll_for(p, timeout, POLLOUT);
}

int wait_for_data(meta_socket p, int timeout)
{
	assert(p != NULL);

	return sock_poll_for(p, timeout, POLLIN);
}

int sock_write(meta_socket p, const char *buf, size_t count, int timeout, int nretries)
{
	ssize_t nwritten = 0;

	assert(p != NULL);
	assert(p->fd >= 0);
	assert(buf != NULL);
	assert(timeout >= 0);
	assert(nretries >= 0);


	do {
		if (!wait_for_writability(p, timeout)) {
			if (errno != EAGAIN)
				return 0;

			// We got EAGAIN, so retry.
			continue;
		}

		if ((nwritten = write(p->fd, buf, count)) == -1) {
			perror("write");
			return 0;
		}

		if (nwritten != (ssize_t)count) {
			buf += nwritten;
			count -= nwritten;
		}
	} while(count > 0 && nretries--);

	/* If not able to write and no errors detected, we have a timeout */
	if ((ssize_t)count != nwritten) {
		errno = EAGAIN;
		return 0;
	}

	return 1;
}

/*
 * read UP TO AND INCLUDING count bytes off the socket.
 *	The rules are:
 *	1. We poll with a timeout of timeout
 *	2. We retry for nretries times.
 *	The reason for this is:
 *	If the # of bytes requested are > one packet and poll()
 *	returns when the first packet returns, we must retry to
 *	get the second packet.
 */
ssize_t sock_read(
	meta_socket p,
	char *dest,
	size_t count,
	int timeout,
	int nretries)
{
	ssize_t nread, nreadsum = 0;

	assert(p != NULL);
	assert(p->fd >= 0);
	assert(timeout >= 0);
	assert(nretries >= 0);
	assert(dest != NULL);

	do {
		size_t cbToRead = count - nreadsum;

		if (!wait_for_data(p, timeout)) {
			if (errno == EAGAIN) {
				continue; // Try again.
			}

			return -1;
		}

		/*
		 * We have data! We don't know how much we have, but we 
		 * do know that the socket is non-blocking. Therefore, let's
		 * spin-read until read() returns 0. 
		 */
		while ((nread = read(p->fd, &dest[nreadsum], cbToRead)) > 0) {
			nreadsum += nread;
			if (nreadsum == count) {
				return nreadsum; // we're done
			}

			cbToRead = count - nreadsum;
		}

		if (nread == -1 && errno != EAGAIN) {
			/* An error occured. Uncool. */
			return -1;
		}
	} while(nreadsum < count && nretries--);

	return nreadsum;
}


/* Binds a socket to an address/port.
 * This part is a bit incorrect as we have already
 * created a socket with a specific protocol family,
 * and here we bind it to the PF specified in the services...
 */
static int sock_bind_inet(meta_socket p, const char *hostname, int port)
{
	struct hostent* host = NULL;
	struct sockaddr_in my_addr;
	socklen_t cb = (socklen_t)sizeof my_addr;

	assert(p != NULL);
	assert(!p->unix_socket);
	assert(p->fd >= 0);
	assert(port > 0);

	if (hostname != NULL) {
		if ((host = gethostbyname(hostname)) ==NULL) {
			errno = h_errno; /* OBSOLETE? */
			return 0;
		}
	}

	memset(&my_addr, '\0', sizeof my_addr);
	my_addr.sin_port = htons(port);

	if (hostname == NULL) {
		my_addr.sin_family = AF_INET;
		my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else {
		my_addr.sin_family = host->h_addrtype;
		my_addr.sin_addr.s_addr = ((struct in_addr*)host->h_addr)->s_addr;
	}

	if (bind(p->fd, (struct sockaddr *)&my_addr, cb) == -1)
		return 0;

	return 1;
}

static int sock_bind_unix(meta_socket p, const char *path)
{
	struct sockaddr_un my_addr;
	socklen_t cb = (socklen_t)sizeof(my_addr);

	assert(p != NULL);
	assert(p->unix_socket);
	assert(p->fd >= 0);
	assert(path != NULL);
	assert(strlen(path) > 0);

	// + 1 in case we map anon paths. That requires an extra byte.
	assert(strlen(path) + 1 < sizeof my_addr.sun_path);

	memset(&my_addr, '\0', sizeof(my_addr));
	my_addr.sun_family = AF_UNIX;

	if (*path == '/')
		strcpy(my_addr.sun_path, path);
	else
		strcpy(my_addr.sun_path + 1, path);

	if (bind(p->fd, (struct sockaddr *)&my_addr, cb) == -1)
		return 0;

	return 1;
}

int sock_bind(meta_socket p, const char *hostname, int port)
{
	assert(p != NULL);

	if (p->unix_socket)
		return sock_bind_unix(p, hostname);
	else
		return sock_bind_inet(p, hostname, port);
}

meta_socket sock_socket(int unix_socket)
{
	meta_socket p;
	int af = unix_socket ? AF_UNIX : AF_INET;

	if ((p = malloc(sizeof *p)) == NULL)
		return NULL;

	if ((p->fd = socket(af, SOCK_STREAM, 0)) == -1) {
		free(p);
		return NULL;
	}

	p->unix_socket = unix_socket;
	return p;
}

int sock_listen(meta_socket p, int backlog)
{
	assert(p != NULL);
	assert(p->fd >= 0);

	if (listen(p->fd, backlog) == -1)
		return 0;

	return 1;
}

meta_socket create_server_socket(int unix_socket, const char *host, int port)
{
	meta_socket p;

	if ((p = sock_socket(unix_socket)) == NULL)
		return NULL;

	if (!sock_set_reuseaddr(p)
	|| !sock_bind(p, host, port)
	|| !sock_listen(p, 100)) {
		sock_close(p);
		return NULL;
	}

	return p;
}

meta_socket create_client_socket(const char *host, int port)
{
	struct hostent *phost;
	struct sockaddr_in sa;
	meta_socket p;

	if ((phost = gethostbyname(host)) == NULL) {
		errno = h_errno; /* OBSOLETE? */
		return 0;
	}

	/* Create a socket structure */
	sa.sin_family = phost->h_addrtype;
	memcpy(&sa.sin_addr, phost->h_addr, (size_t)phost->h_length);
	sa.sin_port = htons(port);

	/* Open a socket to the server */
	if ((p = sock_socket(0)) == NULL)
		return NULL;

	/* Connect to the server. */
	if (connect(p->fd, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
		sock_close(p);
		return NULL;
	}

	if (!sock_set_nonblock(p)) {
		sock_close(p);
		return NULL;
	}

	return p;
}

int sock_set_nonblock(meta_socket p)
{
	int flags;

	assert(p != NULL);
	assert(p->fd >= 0);

	flags = fcntl(p->fd, F_GETFL);
	if (flags == -1)
		return 0;

	flags |= O_NONBLOCK;
	if (fcntl(p->fd, F_SETFL, flags) == -1)
		return 0;

	return 1;
}

int sock_clear_nonblock(meta_socket p)
{
	int flags;

	assert(p != NULL);
	assert(p->fd >= 0);

	flags = fcntl(p->fd, F_GETFL);
	if (flags == -1)
		return 0;

	flags -= (flags & O_NONBLOCK);
	if (fcntl(p->fd, F_SETFL, flags) == -1)
		return 0;

	return 1;
}

int sock_close(meta_socket p)
{
	int fd;

	assert(p != NULL);
	assert(p->fd >= 0);

	fd = p->fd;
	free(p);

	/* shutdown() may return an error from time to time,
	 * e.g. if the client already closed the socket. We then
	 * get error ENOTCONN.
	 * We still need to close the socket locally, therefore
	 * the return code from shutdown is ignored.
	 */
	shutdown(fd, SHUT_RDWR);

	if (close(fd))
		return 0;

	return 1;
}

meta_socket sock_accept(meta_socket p, struct sockaddr *addr, socklen_t *addrsize)
{
	meta_socket newsock;
	int fd;

	assert(p != NULL);
	assert(addr != NULL);
	assert(addrsize != NULL);

	fd = accept(p->fd, addr, addrsize);
	if (fd == -1)
		return NULL;

	if ((newsock = malloc(sizeof *newsock)) == NULL) {
		close(fd);
		return NULL;
	}

	newsock->fd = fd;
	return newsock;
}
