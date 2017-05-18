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
#include <openssl/err.h>

#include <sslsocket.h>

struct sslsocket_tag {
    SSL *ssl;
    BIO *bio;
};

static void ssl_die(sslsocket sock)
{
    ERR_print_errors_fp(stderr);
    if (sock != NULL) {
    }
    exit(1);
}
// This must happen once per server (accept) context.
// At the same time, the defines must go, and their values
// must be provided via an API. That's not hard, but we need to do
// it in a pretty way that doesn't affect the generalized socket
// API too much.
// IRL, where would these values reside? The pem-files would
// of course be very protected and not stored in the bin directory.
// Maybe in /opt/foo/etc, /opt/foo/conf.d or something similar?
// Or maybe on some medium removed after start? Who knows?
// We only know that we need these two files, and optionally
// a path, CADIR, to a directory with trusted certs. 
// We overload some functions so we can do instantaneous verification.
// An alternative could be to add a map/list of "properties".
// For now, we prefer the most deterministic version.
//

#define CADIR    NULL
#define CAFILE   "rootcert.pem"
#define CERTFILE "server.pem"


// Sets the socket options we want on the main socket
// Suitable for server sockets only.
static status_t sslsocket_set_reuseaddr(sslsocket this)
{
    assert(this != NULL);
    assert(this->bio != NULL);

    BIO_set_bind_mode(this->bio, BIO_BIND_REUSEADDR);
    return success;
}

/*
 * This is a local helper function. It polls for some kind of event,
 * which normally is POLLIN or POLLOUT.
 * The function returns 1 if the event has occured, and 0 if an
 * error occured. It set errno to EAGAIN if a timeout occured, and
 * it maps POLLHUP and POLLERR to EPIPE, and POLLNVAL to EINVAL.
 */
status_t sslsocket_poll_for(sslsocket this, int timeout, int poll_for)
{
    status_t status = failure;

    struct pollfd pfd;
    int rc;

    assert(this != NULL);
    assert(poll_for == POLLIN || poll_for == POLLOUT);
    assert(timeout >= 0);

    pfd.fd = BIO_get_fd(this->bio, NULL);
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
    assert(this->ssl != NULL);

    // Are there bytes already read and available?
    if (SSL_pending(this->ssl) > 0)
        return success;

    return sslsocket_poll_for(this, timeout, POLLIN);
}

status_t sslsocket_write(sslsocket this, const char *buf, size_t count, int timeout, int nretries)
{
    ssize_t nwritten = 0;

    assert(this != NULL);
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

        if ((nwritten = SSL_write(this->ssl, buf, count)) == -1)
            return failure;

        buf += nwritten;
        count -= nwritten;
    } while(count > 0 && nretries--);

    /* If not able to write and no errors detected, we have a timeout */
    if (count != 0)
        return fail(EAGAIN);

    return success;
}

ssize_t sslsocket_read(sslsocket this, char *dest, size_t count,
    int timeout, int nretries)
{
    ssize_t nread;

    assert(this != NULL);
    assert(this->ssl != NULL);
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
        if ((nread = SSL_read(this->ssl, dest, count)) > 0)
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
status_t
sslsocket_bind(sslsocket this, const char *hostname, int port)
{
    int rc;

    (void)hostname;
    (void)port;

    // Binds at first call
    rc = BIO_do_accept(this->bio);
    if (rc <= 0) {
        ssl_die(this);
        ERR_print_errors_fp(stderr);
        return failure;
    }

    return success;
}

// TODO: Lots of stuff. We need the SSL_CTX since that one's
// global-ish. I guess the SSL_CTX should belong to
// the tcp_server instance? We don't want to create
// one SSL_CTX per sslsocket. ATM, we only have one
// server class, the tcp_server. That class knows about
// the socktype and holds a socket_t base class instance.
// tcp_server_get_root_resources() calls socket_create_server_socket(),
// it could do more. For example, it could create the
// SSL_CTX and serve that as an argument to this fn.
// An alternative is to virtualize the tcp_server class,
// but that seems overkill when we just need a couple
// of if-statements and some calls.
//
// One issue may be that we don't know the socket type
// at this point. 
sslsocket sslsocket_socket(void)
{
    sslsocket this;

    if ((this = malloc(sizeof *this)) == NULL)
        return NULL;

    return this;
}

status_t sslsocket_listen(sslsocket this, int backlog)
{
#if 0
    assert(this != NULL);
    assert(this->fd >= 0);

    if (listen(this->fd, backlog) == -1)
        return failure;
#else
    (void)this; 
    (void)backlog;
#endif

    return success;
}

sslsocket sslsocket_create_server_socket(const char *host, int port)
{
    sslsocket this;
    char hostport[1024];
    size_t n;

    if (host == NULL)
        host = "localhost";

    n = snprintf(hostport, sizeof hostport, "%s:%d", host, port);
    if (n >= sizeof hostport)
        return NULL;

    if ((this = sslsocket_socket()) == NULL)
        return NULL;

    #if 0
    if (!setup_server_ctx(this)) {
        sslsocket_close(this);
        return NULL;
    }
    #endif

    // Now create the server socket
    this->bio = BIO_new_accept(hostport);

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
    sslsocket this;
    char hostport[1024];
    size_t n;
    int rc;

    assert(host != NULL);

    n = snprintf(hostport, sizeof hostport, "%s:%d", host, port);
    if (n >= sizeof hostport)
        return NULL;

    if ((this = sslsocket_socket()) == NULL)
        return NULL;

    this->bio = BIO_new_connect(hostport);
    if (this->bio == NULL)
        goto err;

    // Set nonblock before connecting. Internet Wisdom(tm)
    if (!sslsocket_set_nonblock(this))
        goto err;

    if (BIO_do_connect(this->bio) <= 0)
        goto err;

    // Note that this->ctx is garbage. We need API changes.
    this->ssl = NULL; // SSL_new(this->ctx);
    if (this->ssl == NULL)
        goto err;

    SSL_set_bio(this->ssl, this->bio, this->bio);
    rc = SSL_connect(this->ssl);
    if (rc <= 0) {
        goto err;
    }

    return this;

err:
    sslsocket_close(this);
    return NULL;
}

// SSLTODO: Read the doc and figure out how to toggle non-block
status_t sslsocket_set_nonblock(sslsocket this)
{
    assert(this != NULL);
    assert(this->bio != NULL);

    BIO_set_nbio(this->bio, 1);
    return success;
}

status_t sslsocket_clear_nonblock(sslsocket this)
{
    assert(this != NULL);
    assert(this->bio != NULL);

    BIO_set_nbio(this->bio, 0);
    return success;
}

status_t sslsocket_close(sslsocket this)
{
    //SSL *ssl;

    assert(this != NULL);
    assert(this->ssl != NULL);

    //SSL_shutdown(this->ssl);
    SSL_free(this->ssl);
    free(this);

    return success;
}

sslsocket sslsocket_accept(sslsocket this, struct sockaddr *addr,
    socklen_t *addrsize)
{
    sslsocket new;
    int rc;

    (void)addr;
    *addrsize = 0; // Mark addr as unused

    assert(this != NULL);
    assert(addr != NULL);
    assert(addrsize != NULL);

    rc = BIO_do_accept(this->bio);
    if (rc <= 0) {
        return NULL;
    }

    if ((new = malloc(sizeof *new)) == NULL) {
        BIO_free(BIO_pop(this->bio)); // Just guessing here...
        return NULL;
    }

    new->bio = BIO_pop(this->bio);
    if (new->bio == NULL) {
        free(new);
        return NULL;
    }

    new->ssl = NULL; // TO make things build: SSL_new(this->ctx);
    if (new->ssl == NULL) {
        free(new);
        return NULL; // Big leak, I know
    }

    SSL_set_accept_state(new->ssl);
    SSL_set_bio(new->ssl, new->bio, new->bio);
    rc = SSL_accept(new->ssl);
    if (rc <= 0) {
        // Here we probably want to check for WANT_READ and WANT_WRITE
        ssl_die(new);
    }

    return new;
}

