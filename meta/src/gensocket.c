#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/poll.h>

#include <gensocket.h>

#include <tcpsocket.h>
#include <sslsocket.h>

struct gensocket_tag {
    int socktype;
    int fd; // As for now, 20251016, we get a copy from the sub-classes. 
    void *instance;

    // Some function pointers we need.
    status_t (*close)(void *instance);

    status_t (*wait_for_data)(void *instance, int timeout);
    status_t (*write)(void *instance, const char *s, size_t count, int timeout, int retries);
    ssize_t  (*read)(void *instance, char *buf, size_t count, int timeout, int retries);
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

static socket_t create_instance(int socktype)
{
    socket_t p;

    assert(socktype == SOCKTYPE_TCP || socktype ==SOCKTYPE_SSL);

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    p->socktype = socktype;

    // Set up the type-specific function pointers
    if (socktype == SOCKTYPE_TCP) {
        p->close = (typeof(p->close))tcpsocket_close;
        p->wait_for_data = (typeof(p->wait_for_data))tcpsocket_wait_for_data;
        p->write = (typeof(p->write))tcpsocket_write;
        p->read = (typeof(p->read))tcpsocket_read;
    }
    else {
        p->close = (typeof(p->close))sslsocket_close;
        p->wait_for_data = (typeof(p->wait_for_data))sslsocket_wait_for_data;
        p->write = (typeof(p->write))sslsocket_write;
        p->read = (typeof(p->read))sslsocket_read;
    }

    return p;
}

// A ctor function
socket_t socket_create_server_socket(int type, const char *host, int port)
{
    socket_t this;

    this = create_instance(type);
    if (this == NULL)
        return NULL;

    if (type == SOCKTYPE_TCP)
        this->instance = tcpsocket_create_server_socket(host, port);
    else
        this->instance = sslsocket_create_server_socket(host, port);

    if (this->instance == NULL) {
        free(this);
        return NULL;
    }

    // This is a nice place to grab the fd from the 'sub-classes' and 
    // store it in socket_t. boa@20251016
    if (type == SOCKTYPE_TCP)
        this->fd = tcpsocket_get_fd((tcpsocket)this->instance);
    else
        this->fd = sslsocket_get_fd((sslsocket)this->instance);
    return this;
}

// A ctor function
socket_t socket_create_client_socket(int socktype, void *context, 
    const char *host, int port)
{
    socket_t this;

    assert(socktype == SOCKTYPE_TCP || context != NULL);

    if ((this = create_instance(socktype)) == NULL)
        return NULL;

    if (socktype == SOCKTYPE_TCP)
        this->instance = tcpsocket_create_client_socket(host, port);
    else
        this->instance = sslsocket_create_client_socket(context, host, port);

    if (this->instance == NULL) {
        free(this);
        return NULL;
    }

    return this;
}

// Accept is special and is also a ctor like function. This function is called from
// tcp_server's accept_new_connections() and deals with TLS specific cruft.
//
// Fun fact: Now that BIO is out, ssl_socket_accept() is borderline equal to tcp_socket_accept().
// Let's see if we can reuse code. 
//
// 20251010: context can be NULL for TCP sockets, but not for SSL
socket_t socket_accept(socket_t p, void *context, struct sockaddr *addr, socklen_t *addrsize)
{
    socket_t new;

    assert(p->socktype == SOCKTYPE_TCP || context != NULL);

    new = create_instance(p->socktype);
    if (new == NULL)
        return NULL;

    if (p->socktype == SOCKTYPE_TCP)
        new->instance = tcpsocket_accept(p->instance, addr, addrsize);
    else
        new->instance = sslsocket_accept(p->instance, context, addr, addrsize);

    if (new->instance == NULL) {
        free(new);
        return NULL;
    }

    return new;
}

status_t socket_close(socket_t p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    status_t rc = p->close(p->instance);
    free(p);
    return rc;
}

status_t socket_wait_for_data(socket_t p, int timeout)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    // TLS calls SSL_pending()!
    return p->wait_for_data(p->instance, timeout);
}

status_t socket_write(socket_t p, const char *s, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->write(p->instance, s, count, timeout, retries);
}

ssize_t socket_read(socket_t p, char *buf, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->read(p->instance, buf, count, timeout, retries);
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


/* Binds a socket to an address/port.
 * This part is a bit incorrect as we have already created a socket with
 * a specific protocol family, and here we bind it to the PF specified
 * in the services...
 *
 * TODO: Make it version agnostic and (later).
 */
status_t gensocket_bind_inet(int fd, const char *hostname, int port)
{
    struct hostent* host = NULL;
    struct sockaddr_in my_addr;
    socklen_t cb = (socklen_t)sizeof my_addr;

    assert(fd >= 0);
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

    if (bind(fd, (struct sockaddr *)&my_addr, cb) == -1)
        return failure;

    return success;
}

