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
#include <unistd.h>
#include <stdio.h> 
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>	
#include <fcntl.h>

#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif


#include <meta_socket.h>


/* local helper functions */
static int sock_poll_for(int fd, int timeout, int poll_for);
static int sock_set_reuseaddr(int sock);

int wait_for_writability(int fd, int timeout)
{
	return sock_poll_for(fd, timeout, POLLOUT);
}

int wait_for_data(int fd, int timeout)
{
	return sock_poll_for(fd, timeout, POLLIN);
}

int sock_write(int fd, const char* s, size_t cbToWrite, int timeout, int cRetries)
{
	ssize_t cbWritten = 0;

	assert(fd >= 0);
	assert(s != NULL);
	assert(timeout >= 0);
	assert(cRetries >= 0);


	do {
		if(!wait_for_writability(fd, timeout)) {
			if(errno != EAGAIN)
				return 0;
		} 
		else if( (cbWritten = write(fd, s, cbToWrite)) == -1) {
			perror("write");
			return 0;
		} 
		else if(cbWritten != (ssize_t)cbToWrite) {
			s += cbWritten;
			cbToWrite -= cbWritten;
		}

	} while((ssize_t)cbToWrite != cbWritten && cRetries--);

	/* If not able to write and no errors detected, we have a timeout */
	if((ssize_t)cbToWrite != cbWritten) {
		errno = EAGAIN;
		return 0;
	}

	return 1;
}

/**
 * This is a local helper function. It polls for some kind of event,
 * which normally is POLLIN or POLLOUT. 
 * The function returns 1 if the event has occured, and 0 if an
 * error occured. It set errno to EAGAIN if a timeout occured, and 
 * it maps POLLHUP and POLLERR to EPIPE, and POLLNVAL to EINVAL. 
 */
static int sock_poll_for(int fd, int timeout, int poll_for)
{
	struct pollfd pfd;
	int rc;
	int status = 0;

	assert(fd >= 0);
	assert(poll_for == POLLIN || poll_for == POLLOUT);
	assert(timeout >= 0);

	pfd.fd = fd;
	pfd.events = (short)poll_for;

	/* NOTE: poll is XPG4, not POSIX */
	rc = poll(&pfd, 1, timeout);
	if(rc == 1) {
		/* We have info in pfd */
		if(pfd.revents & POLLHUP) 
			errno = EPIPE;
		else if(pfd.revents & POLLERR)
			errno = EPIPE;
		else if(pfd.revents & POLLNVAL) 
			errno = EINVAL;
		else if((pfd.revents & poll_for)  == poll_for)
			status = 1;
	}
	else if (rc == 0) 
		errno = EAGAIN;

	return status;
}
/* read UP TO AND INCLUDING cb bytes off the socket.
 *  The rules are:
 *  1. We poll with a timeout of timeout
 *  2. We retry for cRetries times.
 *  The reason for this is:
 *  If the # of bytes requested are > one packet and poll()
 *  returns when the first packet returns, we must retry to
 *  get the second packet.
 */

/* NOTE that sock_read() will return 1 even if zero bytes were read.
 */
int sock_read(
	int fd,
	char *buf,
	size_t cbMax,
	int timeout,
	int cRetries,
	size_t* cbReadSum)
{
	ssize_t cbRead = 0;
	size_t cbToRead;

	assert(fd >= 0);
	assert(timeout >= 0);
	assert(cRetries >= 0);
	assert(buf != NULL);
	assert(cbReadSum != NULL);

	*cbReadSum = 0;
	do {
		cbToRead = cbMax - *cbReadSum;

		if(!wait_for_data(fd, timeout)) {
			if(errno != EAGAIN)
				return 0;
		}
		else if( (cbRead = read(fd, &buf[*cbReadSum], cbToRead)) == -1) {
			/*
			 * We were unable to read data from the socket. 
			 * Inform the caller by returning 0.
			 */
			return 0;
		}
		else if(cbRead > 0) {
			/* We read at least one byte off the socket. */
			*cbReadSum += cbRead;

			/* NOTE: This does not quite cut it, as servers will
			 * block even when there's no more data to read. That
			 * happens when we got all the data in the first call
			 * to read(). So what to do? 
			 * 1. We can call poll() with a tiny timeout just to
			 * see if there's more data available. If it is, increase
			 * cRetries.
			 * 2. We can retrieve the MTU from the OS and check if
			 * MTU == cbRead. That will not support fragmentation, though.
			 * 3. We can specify separate values for retry for servers 
			 * and clients. That's what we do now, but huge downloads
			 * to clients requires lots of retries.
			 */
			//cRetries++;
		}

	} while(*cbReadSum < cbMax && cRetries--);

	return 1;
}


/* Binds a socket to an address/port.
 * This part is a bit incorrect as we have already
 * created a socket with a specific protocol family,
 * and here we bind it to the PF specified in the services...
 */
int sock_bind(int sock, const char* hostname, int port)
{
	struct hostent* host = NULL;
	struct sockaddr_in my_addr;
	socklen_t cb = (socklen_t)sizeof(my_addr);

	assert(port > 0);

	if(hostname != NULL) {
		if( (host = gethostbyname(hostname)) ==NULL) {
			errno = h_errno; /* OBSOLETE? */
			return 0;
		}
	}

	memset(&my_addr, '\0', sizeof(my_addr));
	my_addr.sin_port = htons(port);
		
	if(hostname == NULL) {
		my_addr.sin_family = AF_INET;
		my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else {
		my_addr.sin_family = host->h_addrtype;
		my_addr.sin_addr.s_addr = ((struct in_addr*)host->h_addr)->s_addr;
	}

	if(bind(sock, (struct sockaddr *)&my_addr, cb) == -1)
		return 0;

	return 1;
}

int sock_socket(int* sock)
{
	if( (*sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		return 0;
	else
		return 1;
}

int sock_listen(int sock, int backlog)
{
	if(listen(sock, backlog) == -1)
		return 0;
	else
		return 1;
}

int create_server_socket(int *psock, const char* host, int port)
{
	if(sock_socket(psock)) {
		if(!sock_set_reuseaddr(*psock)
		|| !sock_bind(*psock, host, port)
		|| !sock_listen(*psock, 100)) {
			close(*psock);
			return 0;
		}
		else 
			return 1;
	}
	else 
		return 0;
}

int create_client_socket(int *psock, const char* host, int port)
{
    struct hostent *phost;
    struct sockaddr_in sa;

    if( (phost = gethostbyname(host)) == NULL) {
		errno = h_errno; /* OBSOLETE? */
		return 0;
	}

    /* Create a socket structure */
    sa.sin_family = phost->h_addrtype;
	memcpy(&sa.sin_addr, phost->h_addr, (size_t)phost->h_length);
    sa.sin_port = htons(port);

    /* Open a socket to the server */
	*psock =socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(-1 == *psock) 
		return 0;

    /* Connect to the server. */
    if(connect(*psock, (struct sockaddr *) &sa, sizeof(sa)) == -1) {
		close(*psock);
		return 0;
	}
	else
		return 1;
}

int sock_set_nonblock(int sock)
{
	int flags;

	flags = fcntl(sock, F_GETFL); 
	if(flags == -1) 
		return 0;

	flags |= O_NONBLOCK;
	if(fcntl(sock, F_SETFL, flags) == -1)
		return 0;

	return 1;
}

int sock_clear_nonblock(int sock)
{
	int flags;

	flags = fcntl(sock, F_GETFL);
	if(flags == -1)
		return 0;

	flags -= (flags & O_NONBLOCK);
	if(fcntl(sock, F_SETFL, flags) == -1)
		return 0;
	else
		return 1;
}

/* 
 * Sets the socket options we want on the main socket 
 * Suitable for server sockets only.
 */
static int sock_set_reuseaddr(int sock)
{
	int optval;
	socklen_t optlen;

	optval = 1;
	optlen = (socklen_t)sizeof(optval);
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) == -1)
		return 0;
	else
		return 1;
}

int sock_close(int fd)
{
	/* shutdown() may return an error from time to time,
	 * e.g. if the client already closed the socket. We then
	 * get error ENOTCONN. 
	 * We still need to close the socket locally, therefore
	 * the return code from shutdown is ignored.
	 */
	shutdown(fd, SHUT_RDWR);

	if(close(fd)) 
		return 0;
	else
		return 1;
}

