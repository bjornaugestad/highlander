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

static status_t ssl_write(int fd, SSL *ssl, const char *buf, size_t count,
    int timeout, int nretries)
{
    size_t nwritten = 0;

    assert(ssl != NULL);
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

        int rc = SSL_write_ex(ssl, buf, count, &nwritten);
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
        int err = SSL_get_error(ssl, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                if (!socket_poll_for(fd, timeout, POLLIN))
                    return failure;
                break;

            case SSL_ERROR_WANT_WRITE:
                if (!socket_poll_for(fd, timeout, POLLOUT))
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

status_t socket_poll_for(int fd, int timeout, short poll_for)
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
    assert(socktype == SOCKTYPE_TCP || socktype ==SOCKTYPE_SSL);

    socket_t new = calloc(1, sizeof *new);
    if (new == NULL)
        return NULL;

    new->ssl = NULL; // Will be set later.
    new->socktype = socktype;
    return new;
}

static void socket_free(socket_t this)
{
    if (this != NULL) {
        if (this->ssl != NULL) {
            SSL_free(this->ssl);
        }

        free(this);
    }
}

// Binds a socket to an address
static status_t socket_bind_inet(int fd, struct addrinfo *ai)
{
    if (bind(fd, ai->ai_addr, ai->ai_addrlen) == -1)
        return failure;

    return success;
}

// Create a socket object and call socket()
static socket_t socket_socket(int socktype, struct addrinfo *ai)
{
    assert(socktype == SOCKTYPE_TCP || socktype == SOCKTYPE_SSL);

    socket_t new = socket_new(socktype);
    if (new == NULL)
        return NULL;

    new->fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (new->fd == -1) {
        socket_free(new);
        return NULL;
    }

    return new;
}

static socket_t tcp_create_server_socket(const char *host, uint16_t port)
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
        new = socket_socket(SOCKTYPE_TCP, ai);
        if (new == NULL)
            continue;

        if (socket_set_reuse_addr(socket_get_fd(new))
        && socket_bind_inet(socket_get_fd(new), ai)
        && socket_listen(socket_get_fd(new), 100)) {
            freeaddrinfo(res);
            return new;
        }
    }

    freeaddrinfo(res);
    if (new == NULL)
        return NULL;;

    socket_close(new);
    return NULL;
}

static socket_t tcp_create_client_socket(const char *host, uint16_t port)
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
        this = socket_socket(SOCKTYPE_TCP, ai);
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

    if (!socket_set_nonblock(this->fd)) {
        socket_close(this);
        return NULL;
    }

    return this;
}

// A ctor function
socket_t socket_create_server_socket(int socktype, const char *host, uint16_t port)
{
    socket_t new;
    assert(socktype == SOCKTYPE_TCP || socktype == SOCKTYPE_SSL);

    new = tcp_create_server_socket(host, port);
    if (new != NULL)
        new->socktype = socktype;

    return new;
}

// A ctor function
socket_t socket_create_client_socket(int socktype, void *context, 
    const char *host, uint16_t port)
{
    assert(socktype == SOCKTYPE_TCP || context != NULL);

    if (socktype == SOCKTYPE_TCP)
        return tcp_create_client_socket(host, port);
    else { 
        // SSL - work in progress. BIO related...
        return NULL;
    }
}

__attribute__((nonnull, warn_unused_result))
static socket_t tcp_accept(int fd, struct sockaddr_storage *addr, socklen_t *addrsize)
{
    assert(fd >= 0);
    assert(addr != NULL);
    assert(addrsize != NULL);

    int clientfd = accept4(fd, (struct sockaddr *)addr, addrsize, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (clientfd == -1)
        return NULL;

    socket_t new = socket_new(SOCKTYPE_TCP);
    if (new == NULL) {
        close(clientfd);
        return NULL;
    }

    new->fd = clientfd;
    return new;
}

// Accept a new TLS connection, similar to accept().
static socket_t ssl_accept(int fd, void *context, 
    struct sockaddr_storage *addr, socklen_t *addrsize)
{
    assert(addr != NULL);
    assert(context != NULL);
    assert(addrsize != NULL);

    int clientfd = accept4(fd, (struct sockaddr *)addr, addrsize, SOCK_CLOEXEC);
    if (clientfd == -1)
        return NULL;

    socket_t new = socket_new(SOCKTYPE_SSL);
    if (new == NULL) {
        close(clientfd);
        return NULL;
    }

    new->fd = clientfd;

    // TLS stuff. Keep the socket blocked until handshake's done.
    new->ssl = SSL_new(context);
    if (new->ssl == NULL) {
        socket_close(new);
        return NULL;
    }

    SSL_set_fd(new->ssl, clientfd);
    if (SSL_accept(new->ssl) <= 0) {
        socket_close(new);
        return NULL;
    }

    if (!socket_set_nonblock(new->fd)) {
        socket_close(new);
        return NULL;
    }

    return new;
}

socket_t socket_accept(socket_t listener, void *context, struct sockaddr_storage *addr, socklen_t *addrsize)
{
    assert(listener->socktype == SOCKTYPE_TCP || listener->socktype == SOCKTYPE_SSL);
    assert(listener->socktype == SOCKTYPE_TCP || context != NULL);

    if (listener->socktype == SOCKTYPE_TCP)
        return tcp_accept(listener->fd, addr, addrsize);
    else 
        return ssl_accept(listener->fd, context, addr, addrsize);
}

// This function can be called in various states. Think of it as a dtor.
static status_t ssl_close(socket_t this)
{
    // No object or no fd
    if (this == NULL || this->fd == -1)
        return success;

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

    socket_free(this);
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

    status_t rc;
    if (p->socktype == SOCKTYPE_TCP) {
        int fd = p->fd;
        free(p);
        shutdown(fd, SHUT_RDWR);

        rc = close(fd) == 0 ? success : failure;
    }
    else
        rc = ssl_close(p);

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

    return  socket_poll_for(p->fd, timeout, POLLIN);
}

static status_t tcp_write(int fd, const char *buf, size_t count, int timeout, int nretries)
{
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

        ssize_t nwritten = write(fd, buf, count);
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
        return tcp_write(p->fd, src, count, timeout, retries);
    else
        return ssl_write(p->fd, p->ssl, src, count, timeout, retries);
}

static ssize_t ssl_read(int fd, SSL *ssl, char *dest, size_t count, int timeout, int nretries)
{
    assert(fd > -1);
    assert(ssl != NULL);
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
        size_t nread;
        int rc = SSL_read_ex(ssl, dest, count, &nread);
        if (rc > 0)
            return (ssize_t)nread;

        int err = SSL_get_error(ssl, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                if (!socket_poll_for(fd, timeout, POLLIN))
                    return -1;
                break;

            case SSL_ERROR_WANT_WRITE:
                if (!socket_poll_for(fd, timeout, POLLOUT))
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

static ssize_t tcp_read(int fd, char *dest, size_t count, int timeout, int nretries)
{
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
        ssize_t nread = read(fd, dest, count);
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

    if (p->socktype == SOCKTYPE_TCP) {
        return tcp_read(p->fd, buf, count, timeout, retries);
    }
    else
        return ssl_read(p->fd, p->ssl, buf, count, timeout, retries);
}

status_t socket_set_nonblock(int fd)
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

status_t socket_clear_nonblock(int fd)
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

status_t socket_listen(int fd, int backlog)
{
    assert(fd >= 0);

    if (listen(fd, backlog) == -1)
        return failure;

    return success;
}

status_t socket_set_reuse_addr(int fd)
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

