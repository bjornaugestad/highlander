#define _GNU_SOURCE 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/poll.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <gensocket.h>

// Some compile time sanity checks
_Static_assert((in_port_t)-1 > 0, "in_port_t must be unsigned");
_Static_assert(sizeof(in_port_t) == 2, "in_port_t must be 16-bit");

struct gensocket_tag {
    int socktype;
    int fd;
    SSL *ssl;
};

int socket_get_fd(socket_t this)
{
    assert(this != NULL);
    assert(this->socktype == SOCKTYPE_TCP || this->socktype == SOCKTYPE_SSL);

    return this->fd;
}

static status_t ssl_write(socket_t this, const char *buf, size_t count,
    int timeout, int nretries)
{
    size_t nwritten = 0;

    assert(this != NULL);
    assert(this->socktype == SOCKTYPE_SSL);
    assert(this->ssl != NULL);
    assert(this->fd >= 0);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    do {
        if (!socket_poll_for(this, timeout, POLLOUT)) {
            if (errno != EAGAIN)
                return failure;

            // We got EAGAIN, so retry.
            continue;
        }

        int rc = SSL_write_ex(this->ssl, buf, count, &nwritten);
        if (rc == 1 && nwritten == count) {
            // Yay, we wrote everything in one go
            return success;
        }

        if (rc == 1 && nwritten < count) {
            // Partial write, so we must try agan
            buf += nwritten;
            count -= nwritten;
            continue;
        }

        // At this point, rc == 0. Interpret SSL_get_error()
        int err = SSL_get_error(this->ssl, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                if (!socket_poll_for(this, timeout, POLLIN))
                    return failure;
                break;

            case SSL_ERROR_WANT_WRITE:
                if (!socket_poll_for(this, timeout, POLLOUT))
                    return failure;
                break;

            case SSL_ERROR_ZERO_RETURN:
                // Time to quit
                return failure;

            case SSL_ERROR_SYSCALL:
                if (errno == 0)
                    break; // retry
                return failure;

            default:
                errno = EIO;
                return failure;
        }
    } while(count > 0 && nretries--);

    /* If not able to write and no errors detected, we have a timeout */
    if (count != 0)
        return fail(EAGAIN);

    return success;
}

status_t socket_poll_for(socket_t this, int timeout, short poll_for)
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

// Our ctor
socket_t socket_new(int socktype)
{
    assert(socktype == SOCKTYPE_TCP || socktype ==SOCKTYPE_SSL);

    socket_t new = calloc(1, sizeof *new);
    if (new == NULL)
        return NULL;

    new->ssl = NULL; // Will be set later.
    new->socktype = socktype;
    new->fd = -1;
    return new;
}

void socket_free(socket_t this)
{
    if (this != NULL) {
        assert(this->ssl == NULL && "free it in socket_close()");
        assert(this->fd == -1 && "close it in socket_close()");

        free(this);
    }
}

// Binds a socket to an address
static status_t socket_bind_inet(socket_t this, struct addrinfo *ai)
{
    if (bind(this->fd, ai->ai_addr, ai->ai_addrlen) == -1)
        return failure;

    return success;
}

// call socket()
static status_t socket_socket(socket_t this, struct addrinfo *ai)
{
    assert(this != NULL);
    assert(this->socktype == SOCKTYPE_TCP || this->socktype == SOCKTYPE_SSL);

    this->fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (this->fd == -1)
        return failure;

    return success;
}

static status_t tcp_create_server_socket(socket_t sock, const char *host, uint16_t port)
{
    struct addrinfo hints = {0}, *res, *ai;
    char serv[6];

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

    if (getaddrinfo(host, serv, &hints, &res) != 0)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        if (!socket_socket(sock, ai))
            continue;

        if (socket_set_reuse_addr(sock)
        && socket_bind_inet(sock, ai)
        && socket_listen(sock, 100)) {
            freeaddrinfo(res);
            return success;
        }
    }

    // Unable to bind
    freeaddrinfo(res);
    socket_close(sock);
    return failure;
}


static status_t tcp_create_client_socket(socket_t this, const char *host, uint16_t port)
{
    char serv[6];
    struct addrinfo hints = {0}, *res = NULL, *ai;

    assert(this != NULL);
    assert(this->fd == -1);
    assert(this->socktype == SOCKTYPE_TCP || this->socktype == SOCKTYPE_SSL);
    assert(host != NULL);
    assert(port > 0);

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family   = AF_UNSPEC;          // v4/v6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_ADDRCONFIG | AI_NUMERICSERV;

    int rc = getaddrinfo(host, serv, &hints, &res);
    if (rc)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        if (!socket_socket(this, ai))
            continue;

        if (connect(this->fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;

        socket_close(this);
    }

    freeaddrinfo(res);
    if (this->fd == -1)
        return failure;

    if (!socket_set_nonblock(this)) {
        socket_close(this);
        return failure;
    }

    return success;
}

status_t socket_create_server_socket(socket_t sock, const char *host, uint16_t port)
{
    assert(sock != NULL);
    assert(sock->socktype == SOCKTYPE_TCP || sock->socktype == SOCKTYPE_SSL);

    return tcp_create_server_socket(sock, host, port);
}

// A ctor function. Context iss ssl_ctx. We return status_t and use
// the memory in socket_t arg to store fd et. al. It's preallocated.
status_t socket_create_client_socket(socket_t this, void *context, 
    const char *host, uint16_t port)
{
    assert(this != NULL);
    assert(this->socktype == SOCKTYPE_TCP || context != NULL);
    assert(this->fd == -1 && "You're leaking fd's, bro");

    if (this->socktype == SOCKTYPE_TCP)
        return tcp_create_client_socket(this, host, port);
    else { 
        // SSL - work in progress. BIO related...
        return failure;
    }
}

__attribute__((nonnull, warn_unused_result))
static status_t tcp_accept(socket_t listener, socket_t client, struct sockaddr_storage *addr, socklen_t *addrsize)
{
    assert(listener != NULL);
    assert(listener != NULL);
    assert(listener->fd >= 0);
    assert(addr != NULL);
    assert(addrsize != NULL);

    client->fd = accept4(listener->fd, (struct sockaddr *)addr, addrsize, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (client->fd == -1)
        return failure;

    return success;
}

// Accept a new TLS connection, similar to accept().
static status_t ssl_accept(socket_t listener, socket_t client, void *context, 
    struct sockaddr_storage *addr, socklen_t *addrsize)
{
    assert(listener != NULL);
    assert(client != NULL);
    assert(addr != NULL);
    assert(context != NULL);
    assert(addrsize != NULL);

    client->fd = accept4(listener->fd, (struct sockaddr *)addr, addrsize, SOCK_CLOEXEC);
    if (client->fd == -1)
        return failure;

    // TLS stuff. Keep the socket blocked until handshake's done.
    assert(client->ssl == NULL);
    client->ssl = SSL_new(context);
    if (client->ssl == NULL) {
        socket_close(client);
        return failure;
    }

    SSL_set_fd(client->ssl, client->fd);
    if (SSL_accept(client->ssl) <= 0) {
        socket_close(client);
        return failure;
    }

    if (!socket_set_nonblock(client)) {
        socket_close(client);
        return failure;
    }

    return success;
}

status_t socket_accept(socket_t listener, socket_t newsock, void *context,
    struct sockaddr_storage *addr, socklen_t *addrsize)
{
    assert(listener != NULL);
    assert(newsock != NULL);
    assert(listener->socktype == SOCKTYPE_TCP || listener->socktype == SOCKTYPE_SSL);
    assert(listener->socktype == SOCKTYPE_TCP || context != NULL);

    if (listener->socktype == SOCKTYPE_TCP)
        return tcp_accept(listener, newsock, addr, addrsize);
    else 
        return ssl_accept(listener, newsock, context, addr, addrsize);
}

// This function can be called in various states. Think of it as a dtor.
static status_t ssl_close(socket_t this)
{
    // No object or no fd
    if (this == NULL || this->fd == -1) {
        return success;
    }

    assert(this->socktype == SOCKTYPE_TCP || this->socktype == SOCKTYPE_SSL);
    if (this->ssl != NULL) {
        int ret = SSL_shutdown(this->ssl);
        if (ret == 0)
            ret = SSL_shutdown(this->ssl);
    }

    if (this->fd != -1) {
        shutdown(this->fd, SHUT_RDWR);
        close(this->fd);
    }

    SSL_free(this->ssl);
    this->ssl = NULL; // Important as we now reuse the socket_t objects.
    return success;
}


// Notes:
// tcp shutdown() may return an error from time to time, e.g. if the client
// already closed the socket. We then get error ENOTCONN. We still need to close
// the socket locally, therefore the return code from shutdown is ignored.
status_t socket_close(socket_t p)
{
    assert(p != NULL);
    assert(p->socktype == SOCKTYPE_TCP || p->socktype == SOCKTYPE_SSL);

#if 1
    if (p->fd == -1) {
        // We're already in a closed state. Kinda tricky to deal with,
        // as high level objects like http_server calls close regardless
        // of state, e.g. in case of other errors. We let it slide. The
        // alternative is to track state elsewhere too.
        return success;
    }
#endif

    status_t rc;
    if (p->socktype == SOCKTYPE_TCP) {
#if 1
        shutdown(p->fd, SHUT_RDWR);
#else
        // Try to push the TIME_WAIT to the client by not being the active closer
        // shutdown(p->fd, SHUT_RDWR);

        // Drain receive
        char buf[256];
        while (read(p->fd, buf, sizeof buf) > 0)
            ;
        
#endif
        rc = close(p->fd) == 0 ? success : failure;
    }
    else
        rc = ssl_close(p);

    p->fd = -1;
    return rc;
}

static bool sslsocket_pending(socket_t this)
{
    assert(this != NULL);
    assert(this->ssl != NULL);
    assert(this->socktype == SOCKTYPE_SSL);

    return SSL_pending(this->ssl) > 0;
}

status_t socket_wait_for_data(socket_t p, int timeout)
{
    assert(p != NULL);

    if (p->socktype == SOCKTYPE_SSL && sslsocket_pending(p))
        return success;

    return  socket_poll_for(p, timeout, POLLIN);
}

static status_t tcp_write(socket_t this, const char *buf, size_t count, int timeout, int nretries)
{
    assert(this != NULL);
    assert(this->fd >= 0);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    do {
        if (!socket_poll_for(this, timeout, POLLOUT)) {
            if (errno != EAGAIN)
                return failure;

            // We got EAGAIN, so retry.
            continue;
        }

        ssize_t nwritten = write(this->fd, buf, count);
        if (nwritten == -1)
            return failure;

        buf += nwritten;
        count -= (size_t)nwritten;
    } while(count > 0 && nretries--);

    /* If not able to write and no errors detected, we have a timeout */
    if (count != 0)
        return fail(EAGAIN);

    return success;
}

status_t socket_write(socket_t p, const char *src, size_t count, int timeout, int retries)
{
    assert(p != NULL);
    assert(src != NULL);
    assert(p->socktype == SOCKTYPE_TCP || p->socktype == SOCKTYPE_SSL);

    if (p->socktype == SOCKTYPE_TCP)
        return tcp_write(p, src, count, timeout, retries);
    else
        return ssl_write(p, src, count, timeout, retries);
}

static ssize_t ssl_read(socket_t this, char *dest, size_t count, int timeout, int nretries)
{
    assert(this != NULL);
    assert(this->fd > -1);
    assert(this->socktype == SOCKTYPE_SSL);
    assert(this->ssl != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);
    assert(dest != NULL);

    do {
        if (!socket_poll_for(this, timeout, POLLIN)) {
            if (errno == EAGAIN)
                continue; // Try again.

            return -1;
        }

        // Return data asap, even if partial
        size_t nread;
        int rc = SSL_read_ex(this->ssl, dest, count, &nread);
        if (rc > 0)
            return (ssize_t)nread;

        int err = SSL_get_error(this->ssl, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                if (!socket_poll_for(this, timeout, POLLIN))
                    return -1;
                break;

            case SSL_ERROR_WANT_WRITE:
                if (!socket_poll_for(this, timeout, POLLOUT))
                    return -1;
                break;

            case SSL_ERROR_ZERO_RETURN:
                // peer sent close_notify. Time to quit
                return -1;

            case SSL_ERROR_SYSCALL:
                if (errno == 0)
                    break; // We retry

                return -1;

            default:
                errno = EIO;
                return -1;
        }
    } while (nretries--);

    return -1; // We timed out
}

static ssize_t tcp_read(socket_t this, char *dest, size_t count, int timeout, int nretries)
{
    assert(this != NULL);
    assert(this->fd >= 0);
    assert(timeout >= 0);
    assert(nretries >= 0);
    assert(dest != NULL);

    do {
        if (!socket_poll_for(this, timeout, POLLIN)) {
            if (errno == EAGAIN)
                continue; // Try again.

            return -1;
        }

        // Return data asap, even if partial
        ssize_t nread = read(this->fd, dest, count);
        if (nread > 0)
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
    assert(buf != NULL);
    assert(p->socktype == SOCKTYPE_TCP || p->socktype == SOCKTYPE_SSL);

    if (p->socktype == SOCKTYPE_TCP)
        return tcp_read(p, buf, count, timeout, retries);
    else
        return ssl_read(p, buf, count, timeout, retries);
}

status_t socket_set_nonblock(socket_t this)
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

status_t socket_clear_nonblock(socket_t this)
{
    int flags;

    assert(this->fd >= 0);

    flags = fcntl(this->fd, F_GETFL);
    if (flags == -1)
        return failure;

    flags -= (flags & O_NONBLOCK);
    if (fcntl(this->fd, F_SETFL, flags) == -1)
        return failure;

    return success;
}

status_t socket_listen(socket_t this, int backlog)
{
    assert(this != NULL);
    assert(this->fd >= 0);

    if (listen(this->fd, backlog) == -1)
        return failure;

    return success;
}

status_t socket_set_reuse_addr(socket_t this)
{
    assert(this != NULL);
    assert(this->fd >= 0);

    int optval = 1;
    socklen_t optlen = (socklen_t)sizeof optval;
    if (setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) == -1)
        return failure;

    return success;
}

