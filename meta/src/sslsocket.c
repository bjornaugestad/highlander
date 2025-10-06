/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <unistd.h>
#include <stdio.h>
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
 * This is a helper function. It polls for some kind of event,
 * which normally is POLLIN or POLLOUT.
 * The function returns 1 if the event has occured, and 0 if an
 * error occured. It set errno to EAGAIN if a timeout occured, and
 * it maps POLLHUP and POLLERR to EPIPE, and POLLNVAL to EINVAL.
 */
status_t sslsocket_poll_for(sslsocket this, int timeout, short poll_for)
{
    assert(this != NULL);
    assert(poll_for == POLLIN || poll_for == POLLOUT);
    assert(timeout >= 0);

    struct pollfd pfd;
    pfd.fd = BIO_get_fd(this->bio, NULL);
    pfd.events = poll_for;

    /* NOTE: poll is XPG4, not POSIX */
    int rc = poll(&pfd, 1, timeout);
    status_t status = failure;

    if (rc == 1) {
        /* We have info in pfd */
        if (pfd.revents & POLLHUP)
            errno = EPIPE;
        else if (pfd.revents & POLLERR)
            errno = EPIPE;
        else if (pfd.revents & POLLNVAL)
            errno = EINVAL;
        else if ((pfd.revents & poll_for)  == poll_for)
            status = success;
    }
    else if (rc == 0)
        errno = EAGAIN;

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

// 20251005: TODO: Rewrite to handle SSL requests for read/write, 
// just like we do in the read function below.
status_t sslsocket_write(sslsocket this, const char *buf, size_t count,
    int timeout, int nretries)
{
    size_t nwritten = 0;

    assert(this != NULL);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    do {
        if (!sslsocket_poll_for(this, timeout, POLLOUT)) {
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
                if (!sslsocket_poll_for(this, timeout, POLLIN))
                    return failure;
                break;

            case SSL_ERROR_WANT_WRITE:
                if (!sslsocket_poll_for(this, timeout, POLLOUT))
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

ssize_t sslsocket_read(sslsocket this, char *dest, size_t count,
    int timeout, int nretries)
{
    ssize_t nread;
    status_t status;

    assert(this != NULL);
    assert(this->ssl != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);
    assert(dest != NULL);

    do {
        if (!sslsocket_poll_for(this, timeout, POLLIN)) {
            if (errno == EAGAIN)
                continue; // Try again.

            return -1;
        }

        // Return data asap, even if partial
        if ((nread = SSL_read(this->ssl, dest, count)) > 0) {
            return nread;
        }

        int err = SSL_get_error(this->ssl, nread);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                status = sslsocket_poll_for(this, timeout, POLLIN);
                if (status == failure)
                    return -1;
                break;

            case SSL_ERROR_WANT_WRITE:
                status = sslsocket_poll_for(this, timeout, POLLOUT);
                if (status == failure)
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
        return failure;
    }

    return success;
}

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

    // Now create the server socket
    if ((this->bio = BIO_new_accept(hostport)) == NULL) {
        free(this);
        return NULL;
    }

    if (sslsocket_set_reuseaddr(this)
    && sslsocket_bind(this, host, port)
    && sslsocket_listen(this, 100))
        return this;

    sslsocket_close(this);
    return NULL;
}

sslsocket sslsocket_create_client_socket(void *context,
    const char *host, int port)
{
    sslsocket this;
    char hostport[1024];
    size_t n;
    long res;

    assert(host != NULL);

    n = snprintf(hostport, sizeof hostport, "%s:%d", host, port);
    if (n >= sizeof hostport)
        return NULL;

    if ((this = sslsocket_socket()) == NULL)
        return NULL;

    this->bio = BIO_new_ssl_connect(context);
    if (this->bio == NULL)
        goto err;

    res = BIO_set_conn_hostname(this->bio, hostport);
    if (res != 1)
        goto err;

    BIO_get_ssl(this->bio, &this->ssl);

    const char* const PREFERRED_CIPHERS = "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4";
    res = SSL_set_cipher_list(this->ssl, PREFERRED_CIPHERS);
    if (res != 1)
        goto err;

    res = SSL_set_tlsext_host_name(this->ssl, host);
    if (res != 1)
        goto err;

    res = BIO_do_connect(this->bio);
    if (res != 1)
        goto err;

    res = SSL_do_handshake(this->ssl);
    if (res != 1)
        goto err;

    // Do we have a cert?
    X509 *cert = SSL_get_peer_certificate(this->ssl);
    if (cert == NULL)
        goto err;

    if (SSL_get_verify_result(this->ssl) != X509_V_OK)
        die("Meh, could not verify peer\n");

    return this;

err:
    sslsocket_close(this);
    return NULL;
}

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
    assert(this != NULL);
    assert(this->ssl != NULL);
    assert(this->bio != NULL);

    int ret = SSL_shutdown(this->ssl);
    if (ret == 0)
        ret = SSL_shutdown(this->ssl);


    int fd = BIO_get_fd(this->bio, NULL);
    if (fd > 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    SSL_free(this->ssl);
    // BIO_free_all(this->bio);
    free(this);

    return success;
}

sslsocket sslsocket_accept(sslsocket this, void *context, 
    struct sockaddr *addr __attribute__((unused)), socklen_t *addrsize)
{
    sslsocket new;

    *addrsize = 0; // Mark addr as unused

    assert(this != NULL);
    assert(addrsize != NULL);

    if (BIO_do_accept(this->bio) <= 0)
        return NULL;

    if ((new = malloc(sizeof *new)) == NULL) {
        BIO_free(BIO_pop(this->bio)); // Just guessing here...
        return NULL;
    }

    new->bio = BIO_pop(this->bio);
    if (new->bio == NULL) {
        free(new);
        return NULL;
    }

    new->ssl = SSL_new(context);
    if (new->ssl == NULL) {
        BIO_free(new->bio);
        free(new);
        return NULL;
    }

    SSL_set_accept_state(new->ssl);
    SSL_set_bio(new->ssl, new->bio, new->bio);
    if (SSL_accept(new->ssl) <= 0) {
        sslsocket_close(new);
        errno = EPROTO; // so caller doesn't get a random value
        return NULL;
    }

    return new;
}

