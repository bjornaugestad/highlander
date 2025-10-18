/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#define _GNU_SOURCE 1
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

#include <gensocket.h>
#include <sslsocket.h>

struct sslsocket_tag {
    // fd is -1 if it's never been opened.
    int fd;
    SSL *ssl;
};

int sslsocket_get_fd(sslsocket p)
{
    assert(p != NULL);
    return p->fd;
}

status_t sslsocket_wait_for_data(sslsocket this, int timeout)
{
    assert(this != NULL);
    assert(this->ssl != NULL);

    // Are there bytes already read and available?
    if (SSL_pending(this->ssl) > 0)
        return success;

    return socket_poll_for(this->fd, timeout, POLLIN);
}

status_t sslsocket_write(sslsocket this, const char *buf, size_t count,
    int timeout, int nretries)
{
    size_t nwritten = 0;

    assert(this != NULL);
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
                if (!socket_poll_for(this->fd, timeout, POLLIN))
                    return failure;
                break;

            case SSL_ERROR_WANT_WRITE:
                if (!socket_poll_for(this->fd, timeout, POLLOUT))
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

    assert(this != NULL);
    assert(this->fd > -1);
    assert(this->ssl != NULL);
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
        if ((nread = SSL_read(this->ssl, dest, count)) > 0) {
            return nread;
        }

        int err = SSL_get_error(this->ssl, nread);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                if (!socket_poll_for(this->fd, timeout, POLLIN))
                    return -1;
                break;

            case SSL_ERROR_WANT_WRITE:
                if (!socket_poll_for(this->fd, timeout, POLLOUT))
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


sslsocket sslsocket_socket(struct addrinfo *ai)
{
    sslsocket new;

    if ((new = malloc(sizeof *new)) == NULL)
        return NULL;

    new->ssl = NULL;

    if ((new->fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
        free(new);
        return NULL;
    }

    return new;
}

status_t sslsocket_listen(sslsocket this, int backlog)
{
    assert(this != NULL);
    assert(this->fd >= 0);

    return gensocket_listen(this->fd, backlog);
}

sslsocket sslsocket_create_server_socket(const char *host, int port)
{
    sslsocket new = NULL;

    struct addrinfo hints = {0}, *res, *ai;
    char serv[6];

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

    if (getaddrinfo(host, serv, &hints, &res) != 0)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        new = sslsocket_socket(ai);
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

    sslsocket_close(new);
    return NULL;
}

// 20251008: New version, loosely based on a client program chatgpt wrote
// to test the echoserver when nc failed to perform properly. We throw away
// all the old code which used BIOs a lot. Fuck BIO! Worst concept ever.
//
// So what do we do here? We open a socket connection to a server. 
// IOW, we create a 'socket', we 'connect' and we create an SSL object too.
//
// Assumptions: 
// - An SSL context already exist. (SSL_CTX_new() et al)
// - We don't manage signals here.
// - We're IP v4/v6 agnostic
//

sslsocket sslsocket_create_client_socket(void *context,
    const char *host, int port)
{
// It's a PITA to get rid of BIO, so we disable shit for a while boa@20251008
(void)context; (void)host;(void)port;

    assert(context != NULL);
    assert(host != NULL);
    assert(strlen(host) > 0);
    assert(port > 0);

#if 0
    struct addrinfo hints, *res = NULL, *rp;

    // Create a socket. This calls socket() as well. TODO: Perhaps rewrite semantics?
    // Right now, the fn isn't version agnostic.
    sslsocket new = sslsocket_socket();
    if (new == NULL)
        return NULL;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;     // try IPv6 then IPv4 (system order)
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_ADDRCONFIG;

    rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0)
        return NULL;

    for (rp = res; rp; rp = rp->ai_next) {
        if (connect(new->fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            freeaddrinfo(res);
            break; // success
        }

        // If connect fails, we must close and re-open the socket. Meh.
        // This sucks, as we want the next entry in hints, not the previous.
        // Take a break.
        close(new->fd);
        new->fd = socket
    }

    freeaddrinfo(res);
    if (new->fd == -1)
        return NULL;

    // Now we have a valid fd and it's connected to the server
    // Time to create the sslsocket object.
    assert(fd != -1);


    
    
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
#else
    return NULL;
#endif
}


// This function can be called in various states. Think of it as a dtor.
status_t sslsocket_close(sslsocket this)
{
    // No object or no fd
    if (this == NULL || this->fd == -1)
        return success;

    if (this->ssl != NULL) {
        int ret = SSL_shutdown(this->ssl);
        if (ret == 0)
            ret = SSL_shutdown(this->ssl);
    }

    if (this->fd != -1) {
        shutdown(this->fd, SHUT_RDWR);
        close(this->fd);
    }

    if (this->ssl != NULL) {
        // We free after shutting down the fd
        SSL_free(this->ssl);
    }

    free(this);

    return success;
}

// Accept a new TLS connection, similar to accept().
sslsocket sslsocket_accept(sslsocket this, void *context, 
    struct sockaddr_storage *addr, socklen_t *addrsize)
{
    sslsocket new;
    int clientfd;

    assert(this != NULL);
    assert(addr != NULL);
    assert(addrsize != NULL);

    clientfd = accept4(this->fd, (struct sockaddr *)addr, addrsize, SOCK_CLOEXEC );
    if (clientfd == -1)
        return NULL;

    if ((new = malloc(sizeof *new)) == NULL) {
        close(clientfd);
        return NULL;
    }

    new->fd = clientfd;

    // TLS stuff
    new->ssl = SSL_new(context);
    if (new->ssl == NULL) {
        sslsocket_close(new);
        return NULL;
    }

    SSL_set_fd(new->ssl, clientfd);
    if (SSL_accept(new->ssl) <= 0) {
        sslsocket_close(new);
        return NULL;
    }

    if (!gensocket_set_nonblock(new->fd)) {
        sslsocket_close(new);
        return NULL;
    }

    return new;
}

