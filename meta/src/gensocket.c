#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>

#include <gensocket.h>

#include <tcpsocket.h>
#include <sslsocket.h>

struct gensocket_tag {
    int socktype;
    void *instance;

    // Some function pointers we need.
    status_t (*bind)(void *instance, const char *hostname, int port);
    status_t (*listen)(void *instance, int backlog);
    status_t (*close)(void *instance);

    status_t (*poll_for)(void *instance, int timeout, short polltype);
    status_t (*wait_for_data)(void *instance, int timeout);
    status_t (*wait_for_writability)(void *instance, int timeout);
    status_t (*write)(void *instance, const char *s, size_t count, int timeout, int retries);
    status_t (*set_nonblock)(void *instance);
    status_t (*clear_nonblock)(void *instance);
    ssize_t  (*read)(void *instance, char *buf, size_t count, int timeout, int retries);
};

static socket_t create_instance(int socktype)
{
    socket_t p;

    assert(socktype == SOCKTYPE_TCP || socktype ==SOCKTYPE_SSL);

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    p->socktype = socktype;

    // Set up the type-specific function pointers
    if (socktype == SOCKTYPE_TCP) {
        p->bind = (typeof(p->bind))tcpsocket_bind;
        p->listen = (typeof(p->listen))tcpsocket_listen;
        p->close = (typeof(p->close))tcpsocket_close;
        p->poll_for = (typeof(p->poll_for))tcpsocket_poll_for;
        p->wait_for_data = (typeof(p->wait_for_data))tcpsocket_wait_for_data;
        p->wait_for_writability = (typeof(p->wait_for_writability))tcpsocket_wait_for_writability;
        p->write = (typeof(p->write))tcpsocket_write;
        p->set_nonblock = (typeof(p->set_nonblock))tcpsocket_set_nonblock;
        p->clear_nonblock = (typeof(p->clear_nonblock))tcpsocket_clear_nonblock;
        p->read = (typeof(p->read))tcpsocket_read;
    }
    else {
        p->bind = (typeof(p->bind))sslsocket_bind;
        p->listen = (typeof(p->listen))sslsocket_listen;
        p->close = (typeof(p->close))sslsocket_close;
        p->poll_for = (typeof(p->poll_for))sslsocket_poll_for;
        p->wait_for_data = (typeof(p->wait_for_data))sslsocket_wait_for_data;
        p->wait_for_writability = (typeof(p->wait_for_writability))sslsocket_wait_for_writability;
        p->write = (typeof(p->write))sslsocket_write;
        p->set_nonblock = (typeof(p->set_nonblock))sslsocket_set_nonblock;
        p->clear_nonblock = (typeof(p->clear_nonblock))sslsocket_clear_nonblock;
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
socket_t socket_accept(socket_t p, void *context, struct sockaddr *addr, socklen_t *addrsize)
{
    socket_t new;

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

status_t socket_bind(socket_t p, const char *hostname, int port)
{
    assert(p != NULL);
    assert(p->instance != NULL);
    assert(hostname != NULL);
    assert(port > 0);

    return p->bind(p->instance, hostname, port);
}

status_t socket_listen(socket_t p, int backlog)
{
    assert(p != NULL);
    assert(p->instance != NULL);
    assert(backlog > 0);

    return p->listen(p->instance, backlog);
}

status_t socket_close(socket_t p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    status_t rc = p->close(p->instance);
    free(p);
    return rc;
}

status_t socket_poll_for(socket_t p, int timeout, int polltype)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->poll_for(p->instance, timeout, polltype);
}

status_t socket_wait_for_data(socket_t p, int timeout)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->wait_for_data(p->instance, timeout);
}

status_t socket_wait_for_writability(socket_t p, int timeout)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->wait_for_writability(p->instance, timeout);
}

status_t socket_write(socket_t p, const char *s, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->write(p->instance, s, count, timeout, retries);
}

status_t socket_set_nonblock(socket_t p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->set_nonblock(p->instance);
}

status_t socket_clear_nonblock(socket_t p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->clear_nonblock(p->instance);
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

