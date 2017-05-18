#include <assert.h>
#include <stdio.h>
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

    status_t (*poll_for)(void *instance, int timeout, int polltype);
    status_t (*wait_for_data)(void *instance, int timeout);
    status_t (*wait_for_writability)(void *instance, int timeout);
    status_t (*write)(void *instance, const char *s, size_t count, int timeout, int retries);
    status_t (*set_nonblock)(void *instance);
    status_t (*clear_nonblock)(void *instance);
    ssize_t  (*read)(void *instance, char *buf, size_t count, int timeout, int retries);

    // Some function which make sense only to sslsockets.
    // They return true for other socket types.
    status_t (*set_rootcert)(void *instance, const char *path);
    status_t (*set_private_key)(void *instance, const char *path);
    status_t (*set_ciphers)(void *instance, const char *ciphers);
    status_t (*set_ca_directory)(void *instance, const char *path);
};

static socket_t create_instance(int type)
{
    socket_t p;

    assert(type == SOCKTYPE_TCP || type ==SOCKTYPE_SSL);

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    p->type = type;

    // Set up the type-specific function pointers
    if (type == SOCKTYPE_TCP) {
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

        // No implementation
        p->set_rootcert = 0;
        p->set_private_key = 0;
        p->set_ciphers = 0;
        p->set_ca_directory = 0;
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

        p->set_rootcert = (typeof(p->set_rootcert))sslsocket_set_rootcert;
        p->set_private_key = (typeof(p->set_private_key))sslsocket_set_private_key;
        p->set_ciphers = (typeof(p->set_ciphers))sslsocket_set_ciphers;
        p->set_ca_directory = (typeof(p->set_ca_directory))sslsocket_set_ca_directory;
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
socket_t socket_create_client_socket(int type, const char *host, int port)
{
    socket_t this;

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
socket_t socket_socket(int type)
{
    socket_t this;

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
socket_t socket_accept(socket_t p, struct sockaddr *addr, socklen_t *addrsize)
{
    socket_t new;

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

    return p->close(p->instance);
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

status_t socket_set_rootcert(socket_t p, const char *path)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    if (p->set_rootcert == 0)
        return success;

    return p->set_rootcert(p->instance, path);
}

status_t socket_set_private_key(socket_t p, const char *path)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    if (p->set_private_key == 0)
        return success;

    return p->set_private_key(p->instance, path);
}

status_t socket_set_ciphers(socket_t p, const char *ciphers)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    if (p->set_ciphers == 0)
        return success;

    return p->set_ciphers(p->instance, ciphers);
}

status_t socket_set_ca_directory(socket_t p, const char *path)
{
    assert(p != NULL);
    assert(p->instance != NULL);

    if (p->set_ca_directory == 0)
        return success;

    return p->set_ca_directory(p->instance, path);
}

