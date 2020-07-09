/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <unistd.h>
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
//#include <arpa/inet.h>
#endif

#include <tcpsocket.h>

struct tcpsocket_tag {
    int fd;
};

/*
 * Sets the socket options we want on the main socket
 * Suitable for server sockets only.
 */
static int tcpsocket_set_reuseaddr(tcpsocket this)
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
status_t tcpsocket_poll_for(tcpsocket this, int timeout, short poll_for)
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

status_t tcpsocket_wait_for_writability(tcpsocket this, int timeout)
{
    assert(this != NULL);

    return tcpsocket_poll_for(this, timeout, POLLOUT);
}

status_t tcpsocket_wait_for_data(tcpsocket this, int timeout)
{
    assert(this != NULL);

    return tcpsocket_poll_for(this, timeout, POLLIN);
}

status_t tcpsocket_write(tcpsocket this, const char *buf, size_t count, int timeout, int nretries)
{
    ssize_t nwritten = 0;

    assert(this != NULL);
    assert(this->fd >= 0);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    do {
        if (!tcpsocket_wait_for_writability(this, timeout)) {
            if (errno != EAGAIN)
                return failure;

            // We got EAGAIN, so retry.
            continue;
        }

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
ssize_t tcpsocket_read(tcpsocket this, char *dest, size_t count, int timeout, int nretries)
{
    ssize_t nread;

    assert(this != NULL);
    assert(this->fd >= 0);
    assert(timeout >= 0);
    assert(nretries >= 0);
    assert(dest != NULL);

    do {
        if (!tcpsocket_wait_for_data(this, timeout)) {
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
static status_t tcpsocket_bind_inet(tcpsocket this, const char *hostname, int port)
{
    struct hostent* host = NULL;
    struct sockaddr_in my_addr;
    socklen_t cb = (socklen_t)sizeof my_addr;

    assert(this != NULL);
    assert(this->fd >= 0);
    assert(port > 0);

    // TODO: s/gethostbyname/getaddrinfo/
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

status_t tcpsocket_bind(tcpsocket this, const char *hostname, int port)
{
    assert(this != NULL);

    return tcpsocket_bind_inet(this, hostname, port);
}

tcpsocket tcpsocket_socket(void)
{
    tcpsocket this;
    int af = AF_INET;

    if ((this = malloc(sizeof *this)) == NULL)
        return NULL;

    if ((this->fd = socket(af, SOCK_STREAM, 0)) == -1) {
        free(this);
        return NULL;
    }

    return this;
}

status_t tcpsocket_listen(tcpsocket this, int backlog)
{
    assert(this != NULL);
    assert(this->fd >= 0);

    if (listen(this->fd, backlog) == -1)
        return failure;

    return success;
}

tcpsocket tcpsocket_create_server_socket(const char *host, int port)
{
    tcpsocket this;

    if ((this = tcpsocket_socket()) == NULL)
        return NULL;

    if (tcpsocket_set_reuseaddr(this)
    && tcpsocket_bind(this, host, port)
    && tcpsocket_listen(this, 100))
        return this;

    tcpsocket_close(this);
    return NULL;
}

tcpsocket tcpsocket_create_client_socket(const char *host, int port)
{
    struct hostent *phost;
    struct sockaddr_in sa;
    tcpsocket this;

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
    if ((this = tcpsocket_socket()) == NULL)
        return NULL;

    /* Connect to the server. */
    if (connect(this->fd, (struct sockaddr *) &sa, sizeof sa) == -1) {
        tcpsocket_close(this);
        return NULL;
    }

    if (!tcpsocket_set_nonblock(this)) {
        tcpsocket_close(this);
        return NULL;
    }

    return this;
}

status_t tcpsocket_set_nonblock(tcpsocket this)
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

status_t tcpsocket_clear_nonblock(tcpsocket this)
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

status_t tcpsocket_close(tcpsocket this)
{
    int fd;

    assert(this != NULL);
    assert(this->fd >= 0);

    fd = this->fd;
    free(this);

    // shutdown() may return an error from time to time, e.g. if the client
    // already closed the socket. We then get error ENOTCONN. We still
    // need to close the socket locally, therefore the return code
    // from shutdown is ignored.
    shutdown(fd, SHUT_RDWR);

    if (close(fd))
        return failure;

    return success;
}

tcpsocket tcpsocket_accept(tcpsocket this, struct sockaddr *addr, socklen_t *addrsize)
{
    tcpsocket new;
    int clientfd;

    assert(this != NULL);
    assert(addr != NULL);
    assert(addrsize != NULL);

    clientfd = accept(this->fd, addr, addrsize);
    if (clientfd == -1)
        return NULL;

    if ((new = malloc(sizeof *new)) == NULL) {
        close(clientfd);
        return NULL;
    }

    new->fd = clientfd;
    // We set the nonblock flag here, since SSL prefers to set this
    // early and we want tcp_server to treat sockets uniformly.
    if (!tcpsocket_set_nonblock(new)) {
        close(clientfd);
        free(new);
        return NULL;
    }

    return new;
}
