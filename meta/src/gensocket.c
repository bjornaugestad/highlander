#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/poll.h>

#include <gensocket.h>

#include <tcpsocket.h>
#include <sslsocket.h>

struct gensocket_tag {
    int socktype;
    int fd; // As for now, 20251016, we get a copy from the sub-classes. 
    void *instance;
};

int socket_get_fd(socket_t this)
{
    assert(this != NULL);
    return this->fd;
}

status_t socket_poll_for(int fd, int timeout, int poll_for)
{
    struct pollfd pfd;
    int rc;
    status_t status = failure;

    assert(fd >= 0);
    assert(poll_for == POLLIN || poll_for == POLLOUT);
    assert(timeout >= 0);

    pfd.fd = fd;
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

// Our ctor
static socket_t socket_new(int socktype)
{
    socket_t p;

    assert(socktype == SOCKTYPE_TCP || socktype ==SOCKTYPE_SSL);

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    p->socktype = socktype;

    return p;
}

static status_t tcp_create_server_socket(socket_t this, const char *host, int port)
{
    socket_t new = NULL;

    struct addrinfo hints = {0}, *res, *ai;
    char serv[6];

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

    if (getaddrinfo(host, serv, &hints, &res) != 0)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        new = socket_socket(ai);
        if (new == NULL)
            continue;

        if (gensocket_set_reuse_addr(socket_get_fd(new))
        && gensocket_bind_inet(socket_get_fd(new), ai)
        && gensocket_listen(socket_get_fd(new), 100)) {
            freeaddrinfo(res);
            *this = *new;
            return success;
        }
    }

    freeaddrinfo(res);
    if (new == NULL)
        return failure;

    socket_close(new);
    return failure;
}

static socket_t tcp_create_client_socket(const char *host, int port)
{
    char serv[6];
    struct addrinfo hints = {0}, *res = NULL, *ai;
    socket_t this = NULL;

    assert(host != NULL);

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family   = AF_UNSPEC;          // v4/v6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_ADDRCONFIG | AI_NUMERICSERV;

    int rc = getaddrinfo(host, serv, &hints, &res);
    if (rc)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        this = socket_socket(ai);
        if (this == NULL)
            continue;

        if (connect(socket_get_fd(this), ai->ai_addr, ai->ai_addrlen) == 0)
            break;

        socket_close(this);
        this = NULL;
    }

    freeaddrinfo(res);
    if (this == NULL)
        return NULL;

    if (!gensocket_set_nonblock(socket_get_fd(this))) {
        socket_close(this);
        return NULL;
    }

    return this;
}

// A ctor function
socket_t socket_create_server_socket(int type, const char *host, int port)
{
    socket_t this;
    status_t rc;

    this = socket_new(type);
    if (this == NULL)
        return NULL;

    if (type == SOCKTYPE_TCP)
        rc = tcp_create_server_socket(this, host, port);
    else
        this->instance = sslsocket_create_server_socket(host, port);

    if (this->instance == NULL) {
        free(this);
        return NULL;
    }
    (void)rc;

    // This is a nice place to grab the fd from the 'sub-classes' and 
    // store it in socket_t. boa@20251016
    if (type == SOCKTYPE_SSL)
        this->fd = sslsocket_get_fd((sslsocket)this->instance);

    return this;
}

// A ctor function
socket_t socket_create_client_socket(int socktype, void *context, 
    const char *host, int port)
{
    socket_t new;

    assert(socktype == SOCKTYPE_TCP || context != NULL);

    if (socktype == SOCKTYPE_TCP) {
        if ((new = tcp_create_client_socket(host, port)) != NULL) {
            assert(new->instance == NULL);
            return new;
        }
    }
    else {
        // SSL - work in progress
        if ((new = socket_new(socktype)) == NULL)
            return NULL;

        new->instance = sslsocket_create_client_socket(context, host, port);
    }

    if (new->instance == NULL) {
        free(new);
        return NULL;
    }

    return new;
}
static socket_t tcp_accept(int fd, struct sockaddr_storage *addr, socklen_t *addrsize)
{
    socket_t new;
    int clientfd;

    assert(fd >= 0);
    assert(addr != NULL);
    assert(addrsize != NULL);

    clientfd = accept4(fd, (struct sockaddr *)addr, addrsize, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (clientfd == -1)
        return NULL;

    if ((new = socket_new(SOCKTYPE_TCP)) == NULL) {
        close(clientfd);
        return NULL;
    }

    new->fd = clientfd;
    return new;
}


// Accept is special and is also a ctor like function. This function is called from
// tcp_server's accept_new_connections() and deals with TLS specific cruft.
//
socket_t socket_accept(socket_t listener, void *context, struct sockaddr_storage *addr, socklen_t *addrsize)
{
    socket_t new;

    assert(listener->socktype == SOCKTYPE_TCP || context != NULL);

    new = socket_new(listener->socktype);
    if (new == NULL)
        return NULL;

    if (listener->socktype == SOCKTYPE_TCP)
        new = tcp_accept(listener->fd, addr, addrsize);
    else {
        new->instance = sslsocket_accept(listener->instance, context, addr, addrsize);
        if (new->instance == NULL) {
            free(new);
            return NULL;
        }
    }

    return new;
}

socket_t socket_socket(struct addrinfo *ai)
{
    socket_t this;

    if ((this = calloc(1, sizeof *this)) == NULL)
        return NULL;

    if ((this->fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
        free(this);
        return NULL;
    }

    return this;
}

// Notes:
// tcp shutdown() may return an error from time to time, e.g. if the client
// already closed the socket. We then get error ENOTCONN. We still need to close
// the socket locally, therefore the return code from shutdown is ignored.
status_t socket_close(socket_t p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    status_t rc;
    if (p->socktype == SOCKTYPE_TCP) {
        int fd = p->fd;
        shutdown(fd, SHUT_RDWR);

        rc = close(fd) == 0 ? success : failure;
        free(p->instance);
    }
    else
        rc = sslsocket_close(p->instance);

    free(p);
    return rc;
}

status_t socket_wait_for_data(socket_t p, int timeout)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    if (p->socktype == SOCKTYPE_SSL && sslsocket_pending(p->instance))
        return success;

    return  socket_poll_for(p->fd, timeout, POLLIN);
}

static status_t tcp_socket_write(int fd, const char *buf, size_t count, int timeout, int nretries)
{
    ssize_t nwritten = 0;

    assert(fd >= 0);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    do {
        if (!socket_poll_for(fd, timeout, POLLOUT)) {
            if (errno != EAGAIN)
                return failure;

            // We got EAGAIN, so retry.
            continue;
        }

        if ((nwritten = write(fd, buf, count)) == -1)
            return failure;

        buf += nwritten;
        count -= nwritten;
    } while(count > 0 && nretries--);

    /* If not able to write and no errors detected, we have a timeout */
    if (count != 0)
        return fail(EAGAIN);

    return success;
}

status_t socket_write(socket_t p, const char *src, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(p->instance != NULL);
    assert(src != NULL);

    status_t rc;
    if (p->socktype == SOCKTYPE_TCP)
        rc = tcp_socket_write(p->fd, src, count, timeout, retries);
    else
        rc = sslsocket_write(p->instance, src, count, timeout, retries);

    return rc;
}

static ssize_t tcp_read(int fd, char *dest, size_t count, int timeout, int nretries)
{
    ssize_t nread;

    assert(fd >= 0);
    assert(timeout >= 0);
    assert(nretries >= 0);
    assert(dest != NULL);

    do {
        if (!socket_poll_for(fd, timeout, POLLIN)) {
            if (errno == EAGAIN)
                continue; // Try again.

            return -1;
        }

        // Return data asap, even if partial
        if ((nread = read(fd, dest, count)) > 0)
            return nread;

        if (nread == -1 && errno != EAGAIN) {
            /* An error occured. Uncool. */
            return -1;
        }
    } while (nretries--);

    return -1; // We timed out
}

ssize_t socket_read(socket_t p, char *buf, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(p->instance != NULL);
    assert(buf != NULL);

    ssize_t rc;
    if (p->socktype == SOCKTYPE_TCP) {
        rc = tcp_read(p->fd, buf, count, timeout, retries);
    }
    else
        rc = sslsocket_read(p->instance, buf, count, timeout, retries);

    return rc;
}

status_t gensocket_set_nonblock(int fd)
{
    int flags;

    assert(fd >= 0);

    flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return failure;

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
        return failure;

    return success;
}

status_t gensocket_clear_nonblock(int fd)
{
    int flags;

    assert(fd >= 0);

    flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return failure;

    flags -= (flags & O_NONBLOCK);
    if (fcntl(fd, F_SETFL, flags) == -1)
        return failure;

    return success;
}

status_t gensocket_listen(int fd, int backlog)
{
    assert(fd >= 0);

    if (listen(fd, backlog) == -1)
        return failure;

    return success;
}

status_t gensocket_set_reuse_addr(int fd)
{
    int optval;
    socklen_t optlen;

    assert(fd >= 0);

    optval = 1;
    optlen = (socklen_t)sizeof optval;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) == -1)
        return failure;

    return success;
}


// Binds a socket to an address
status_t gensocket_bind_inet(int fd, struct addrinfo *ai)
{
    if (bind(fd, ai->ai_addr, ai->ai_addrlen) == -1)
        return failure;

    return success;
}

