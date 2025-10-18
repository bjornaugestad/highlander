/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
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

#include <gensocket.h>
#include <tcpsocket.h>

struct tcpsocket_tag {
    int fd;
};

int tcpsocket_get_fd(tcpsocket p)
{
    assert(p != NULL);
    return p->fd;
}

status_t tcpsocket_wait_for_data(tcpsocket this, int timeout)
{
    assert(this != NULL);

    return socket_poll_for(this->fd, timeout, POLLIN);
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
        if (!socket_poll_for(this->fd, timeout, POLLOUT)) {
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
        if (!socket_poll_for(this->fd, timeout, POLLIN)) {
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

static tcpsocket tcpsocket_socket(struct addrinfo *ai)
{
    tcpsocket this;

    if ((this = malloc(sizeof *this)) == NULL)
        return NULL;

    if ((this->fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
        free(this);
        return NULL;
    }

    return this;
}

tcpsocket tcpsocket_create_server_socket(const char *host, int port)
{
    tcpsocket new = NULL;

    struct addrinfo hints = {0}, *res, *ai;
    char serv[6];

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

    if (getaddrinfo(host, serv, &hints, &res) != 0)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        new = tcpsocket_socket(ai);
        if (new == NULL)
            continue;

        if (gensocket_set_reuse_addr(new->fd)
        && gensocket_bind_inet(new->fd, ai)
        && gensocket_listen(new->fd, 100)) {
            freeaddrinfo(res);
            return new;
        }
    }

    freeaddrinfo(res);
    if (new == NULL)
        return NULL;

    tcpsocket_close(new);
    return NULL;
}

tcpsocket tcpsocket_create_client_socket(const char *host, int port)
{
    char serv[6];
    struct addrinfo hints = {0}, *res = NULL, *ai;
    tcpsocket this = NULL;

    assert(host != NULL);

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family   = AF_UNSPEC;          // v4/v6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_ADDRCONFIG | AI_NUMERICSERV;

    int rc = getaddrinfo(host, serv, &hints, &res);
    if (rc)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        this = tcpsocket_socket(ai);
        if (this == NULL)
            continue;

        if (connect(this->fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;

        tcpsocket_close(this);
        this = NULL;
    }

    freeaddrinfo(res);
    if (this == NULL)
        return NULL;

    if (!gensocket_set_nonblock(this->fd)) {
        tcpsocket_close(this);
        return NULL;
    }

    return this;
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
    if (!gensocket_set_nonblock(new->fd)) {
        close(clientfd);
        free(new);
        return NULL;
    }

    return new;
}
