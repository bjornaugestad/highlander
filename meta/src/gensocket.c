#include <assert.h>
#include <stdlib.h>

#include <gensocket.h>

#include <tcpsocket.h>
#include <sslsocket.h>

struct gensocket_tag {
    int type;
    void *instance;

    // Some function pointers we need.
    status_t (*bind)(void *instance, const char *hostname, int port);
    status_t (*listen)(void *instance, int backlog);
    status_t (*close)(void *instance);

    status_t (*wait_for_data)(void *instance, int timeout);
    status_t (*wait_for_writability)(void *instance, int timeout);
    status_t (*write)(void *instance, const char *s, size_t count, int timeout, int retries);
    status_t (*set_nonblock)(void *instance);
    status_t (*clear_nonblock)(void *instance);
    ssize_t  (*read)(void *instance, char *buf, size_t count, int timeout, int retries);
};

static sock create_instance(int type)
{
    sock p;

    assert(type == SOCKTYPE_TCP || type ==SOCKTYPE_SSL);

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    p->type = type;

    // Set up the type-specific function pointers
    if (type == SOCKTYPE_TCP) {
        p->bind = (typeof(p->bind))tcpsocket_bind;
        p->listen = (typeof(p->listen))tcpsocket_listen;
        p->close = (typeof(p->close))tcpsocket_close;
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
sock sock_create_server_socket(int type, const char *host, int port)
{
    sock this;

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
sock sock_create_client_socket(int type, const char *host, int port)
{
    sock this;

    this = create_instance(type);
    if (this == NULL)
        return NULL;

    if (type == SOCKTYPE_TCP)
        this->instance = tcpsocket_create_client_socket(host, port);
    else
        this->instance = sslsocket_create_client_socket(host, port);
    
    if (this->instance == NULL) {
        free(this);
        return NULL;
    }

    return this;
}

// A ctor function
sock sock_socket(int type)
{
    sock this;

    this = create_instance(type);
    if (this == NULL)
        return NULL;

    if (type == SOCKTYPE_TCP)
        this->instance = tcpsocket_socket();
    else
        this->instance = sslsocket_socket();
    
    if (this->instance == NULL) {
        free(this);
        return NULL;
    }

    return this;
}

// Accept is special and is also a ctor like function.
sock sock_accept(sock p, struct sockaddr *addr, socklen_t *addrsize)
{
    sock new;

    new = create_instance(p->type);
    if (new == NULL)
        return NULL;

    if (p->type == SOCKTYPE_TCP)
        new->instance = tcpsocket_accept(p->instance, addr, addrsize);
    else
        new->instance = sslsocket_accept(p->instance, addr, addrsize);
    
    if (new->instance == NULL) {
        free(new);
        return NULL;
    }

    return new;
}

status_t sock_bind(sock p, const char *hostname, int port)
{
    assert(p != NULL);
    assert(p->instance != NULL);
    assert(hostname != NULL);
    assert(port > 0);

    return p->bind(p->instance, hostname, port);
}

status_t sock_listen(sock p, int backlog)
{
    assert(p != NULL);
    assert(p->instance != NULL);
    assert(backlog > 0);

    return p->listen(p->instance, backlog);
}

status_t sock_close(sock p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->close(p->instance);
}


status_t sock_wait_for_data(sock p, int timeout)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->wait_for_data(p->instance, timeout);
}

status_t sock_wait_for_writability(sock p, int timeout)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->wait_for_writability(p->instance, timeout);
}

status_t sock_write(sock p, const char *s, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->write(p->instance, s, count, timeout, retries);
}

status_t sock_set_nonblock(sock p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->set_nonblock(p->instance);
}

status_t sock_clear_nonblock(sock p)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->clear_nonblock(p->instance);
}

ssize_t sock_read(sock p, char *buf, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    return p->read(p->instance, buf, count, timeout, retries);
}

