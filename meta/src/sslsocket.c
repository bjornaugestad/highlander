/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
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
#include <openssl/ssl.h>

#ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet.h>
#endif


#include <sslsocket.h>

// SSLTODO: Extend / change struct to contain all SSL-relevant info for the socket. SSL may be relevant, but not SSL_CTX. Stuff unique to a socket, goes here.
struct sslsocket_tag {
    int fd;
};


/*
 * Sets the socket options we want on the main socket
 * Suitable for server sockets only.
 */
// SSLTODO: Use an SSL way of setting SO_REUSEADDR
static int sslsocket_set_reuseaddr(sslsocket this)
{
    int optval;
    socklen_t optlen;

    assert(this != NULL);
    assert(this->fd >= 0);

    optval = 1;
    optlen = (socklen_t)sizeof optval;
    if (setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) == -1)
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
// SSLTODO: Polling must change, I guess
static status_t sslsocket_poll_for(sslsocket this, int timeout, short poll_for)
{
    struct pollfd pfd;
    int rc;
    status_t status = failure;

    assert(this != NULL);
    assert(this->fd >= 0);
    assert(poll_for == POLLIN || poll_for == POLLOUT);
    assert(timeout >= 0);

    pfd.fd = this->fd;
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
            status = success;
        }
    }
    else if (rc == 0) {
        errno = EAGAIN;
    }
    else if (rc == -1) {
    }

    return status;
}

status_t sslsocket_wait_for_writability(sslsocket this, int timeout)
{
    assert(this != NULL);

    return sslsocket_poll_for(this, timeout, POLLOUT);
}

status_t sslsocket_wait_for_data(sslsocket this, int timeout)
{
    assert(this != NULL);

    return sslsocket_poll_for(this, timeout, POLLIN);
}

status_t sslsocket_write(sslsocket this, const char *buf, size_t count, int timeout, int nretries)
{
    ssize_t nwritten = 0;

    assert(this != NULL);
    assert(this->fd >= 0);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    do {
        if (!sslsocket_wait_for_writability(this, timeout)) {
            if (errno != EAGAIN)
                return failure;

            // We got EAGAIN, so retry.
            continue;
        }

        // SSLTODO: Use SSL_write(), I guess
        if ((nwritten = write(this->fd, buf, count)) == -1)
            return failure;

        buf += nwritten;
        count -= nwritten;
    } while(count > 0 && nretries--);

    /* If not able to write and no errors detected, we have a timeout */
    if (count != 0)
        return fail(EAGAIN);

    return success;
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
 *
 * update 20170514: This sucks. Why? Because the caller doesn't
 * know how many bytes are available. All the caller can do, is
 * to say the _max_ number of bytes to read, i.e., the buffer size.
 * If we require count bytes each time, we end up timing out
 * every damn time, if 'count' is greater than number of bytes
 * available. 
 *
 * Therefore, return whatever we have as soon as possible. If
 * the data is fragmented, then the protocol handler must handle
 * those cases. 
 */
ssize_t sslsocket_read(sslsocket this, char *dest, size_t count, int timeout, int nretries)
{
    ssize_t nread;

    assert(this != NULL);
    assert(this->fd >= 0);
    assert(timeout >= 0);
    assert(nretries >= 0);
    assert(dest != NULL);

    do {
        if (!sslsocket_wait_for_data(this, timeout)) {
            if (errno == EAGAIN)
                continue; // Try again.

            return -1;
        }

        // Return data asap, even if partial
        if ((nread = read(this->fd, dest, count)) > 0)
            return nread;

        if (nread == -1 && errno != EAGAIN) {
            /* An error occured. Uncool. */
            return -1;
        }
    } while (nretries--);

    return -1; // We timed out
}


/* Binds a socket to an address/port.
 * This part is a bit incorrect as we have already
 * created a socket with a specific protocol family,
 * and here we bind it to the PF specified in the services...
 */
static status_t sslsocket_bind_inet(sslsocket this, const char *hostname, int port)
{
    struct hostent* host = NULL;
    struct sockaddr_in my_addr;
    socklen_t cb = (socklen_t)sizeof my_addr;

    assert(this != NULL);
    assert(this->fd >= 0);
    assert(port > 0);

    if (hostname != NULL) {
        if ((host = gethostbyname(hostname)) ==NULL) {
            errno = h_errno; /* OBSOLETE? */
            return failure;
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

    if (bind(this->fd, (struct sockaddr *)&my_addr, cb) == -1)
        return failure;

    return success;
}

status_t sslsocket_bind(sslsocket this, const char *hostname, int port)
{
    assert(this != NULL);

    return sslsocket_bind_inet(this, hostname, port);
}

sslsocket sslsocket_socket(void)
{
    sslsocket this;
    int af = AF_INET;

    if ((this = malloc(sizeof *this)) == NULL)
        return NULL;

    if ((this->fd = socket(af, SOCK_STREAM, 0)) == -1) {
        free(this);
        return NULL;
    }

    return this;
}

status_t sslsocket_listen(sslsocket this, int backlog)
{
    assert(this != NULL);
    assert(this->fd >= 0);

    if (listen(this->fd, backlog) == -1)
        return failure;

    return success;
}

// SSLTODO: We need to initialize the _server_, that'll create an SSL_CTX
// SSLTODO: object. We use that object to create a BIO object. Then we accept
// SSLTODO: connections to the BIO, and create an SSL object using 
// SSLTODO: SSL_new(), SSL_set_accept_state(), SSL_set_bio(). When all
// SSLTODO: that shit's done, we can call SSL_accept(), and do
// SSLTODO: post connection checks on the server side too. (highly optional)
// SSLTODO: 
// SSLTODO: The example programs are good. Go with them
sslsocket sslsocket_create_server_socket(const char *host, int port)
{
    sslsocket this;

    if ((this = sslsocket_socket()) == NULL)
        return NULL;

    if (sslsocket_set_reuseaddr(this)
    && sslsocket_bind(this, host, port)
    && sslsocket_listen(this, 100))
        return this;

    sslsocket_close(this);
    return NULL;
}

// SSLTODO: The SSL connect procedure is more complicated than the normal connect procedure.
// SSLTODO: The sample code in client3.c illustrates this. One first connects a BIO, then 
// SSLTODO: create an SSL object, initializes it with the BIO, then connects the SSL object
// SSLTODO: itself. The client3.c code uses a post connection check which validates the
// SSLTODO: server's certificate. This code is not for the meek. ;)
sslsocket sslsocket_create_client_socket(const char *host, int port)
{
    struct hostent *phost;
    struct sockaddr_in sa;
    sslsocket this;

    assert(host != NULL);

    if ((phost = gethostbyname(host)) == NULL) {
        errno = h_errno; /* OBSOLETE? */
        return NULL;
    }

    /* Create a socket structure */
    sa.sin_family = phost->h_addrtype;
    memcpy(&sa.sin_addr, phost->h_addr, (size_t)phost->h_length);
    sa.sin_port = htons(port);

    /* Open a socket to the server */
    if ((this = sslsocket_socket()) == NULL)
        return NULL;

    /* Connect to the server. */
    if (connect(this->fd, (struct sockaddr *) &sa, sizeof sa) == -1) {
        sslsocket_close(this);
        return NULL;
    }

    if (!sslsocket_set_nonblock(this)) {
        sslsocket_close(this);
        return NULL;
    }

    return this;
}

// SSLTODO: Read the doc and figure out how to toggle non-block
status_t sslsocket_set_nonblock(sslsocket this)
{
    int flags;

    assert(this != NULL);
    assert(this->fd >= 0);

    flags = fcntl(this->fd, F_GETFL);
    if (flags == -1)
        return failure;

    flags |= O_NONBLOCK;
    if (fcntl(this->fd, F_SETFL, flags) == -1)
        return failure;

    return success;
}

status_t sslsocket_clear_nonblock(sslsocket this)
{
    int flags;

    assert(this != NULL);
    assert(this->fd >= 0);

    flags = fcntl(this->fd, F_GETFL);
    if (flags == -1)
        return failure;

    flags -= (flags & O_NONBLOCK);
    if (fcntl(this->fd, F_SETFL, flags) == -1)
        return failure;

    return success;
}

status_t sslsocket_close(sslsocket this)
{
    int fd;

    assert(this != NULL);
    assert(this->fd >= 0);

    fd = this->fd;
    free(this);

    /* shutdown() may return an error from time to time,
     * e.g. if the client already closed the socket. We then
     * get error ENOTCONN.
     * We still need to close the socket locally, therefore
     * the return code from shutdown is ignored.
     */
    // SSLTODO: Use SSL_shutdown
    shutdown(fd, SHUT_RDWR);

    if (close(fd))
        return failure;

    return success;
}

sslsocket sslsocket_accept(sslsocket this, struct sockaddr *addr, socklen_t *addrsize)
{
    sslsocket new;
    int fd;

    assert(this != NULL);
    assert(addr != NULL);
    assert(addrsize != NULL);

    // SSLTODO: Use SSL_accept()?
    fd = accept(this->fd, addr, addrsize);
    if (fd == -1)
        return NULL;

    if ((new = malloc(sizeof *new)) == NULL) {
        // SSLTODO: Use SSL_shutdown()?
        close(fd);
        return NULL;
    }

    // SSLTODO: We need more init stuff here, I guess.
    new->fd = fd;
    return new;
}
