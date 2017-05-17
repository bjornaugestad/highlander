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
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *bio;
};

#define CADIR    NULL
#define CAFILE   "rootcert.pem"
#define CERTFILE "server.pem"
#define CIPHER_LIST "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

static void ssl_die(sslsocket sock)
{
    ERR_print_errors_fp(stderr);
    if (sock != NULL) {
    }
    exit(1);
}

// Copied from sample program.
static int verify_callback(int ok, X509_STORE_CTX *store)
{
    char data[256];

    if (!ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(store);
        int  depth = X509_STORE_CTX_get_error_depth(store);
        int  err = X509_STORE_CTX_get_error(store);

        fprintf(stderr, "-Error with certificate at depth: %i\n", depth);
        X509_NAME_oneline(X509_get_issuer_name(cert), data, 256);
        fprintf(stderr, "  issuer   = %s\n", data);
        X509_NAME_oneline(X509_get_subject_name(cert), data, 256);
        fprintf(stderr, "  subject  = %s\n", data);
        fprintf(stderr, "  err %i:%s\n", err, X509_verify_cert_error_string(err));
    }

    return ok;
}

// Copied from sample program. Needs change.
DH *dh1024 = NULL;

__attribute__((warn_unused_result))
static status_t init_dhparams(void)
{
    BIO *bio;

    bio = BIO_new_file("dh1024.pem", "r");
    if (!bio)
        return failure;

    dh1024 = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    if (!dh1024)
        return failure;

    BIO_free(bio);
    return success;
}

static DH *tmp_dh_callback(SSL *ssl, int is_export, int keylength)
{
    DH *ret;
    (void)ssl;
    (void)is_export;

    if (!dh1024)
        if (!init_dhparams())
            return NULL;

    switch (keylength) {
        case 1024:
        default: /* generating DH params is too costly to do on the fly */
            ret = dh1024;
            break;
    }
    return ret;
}

static status_t setup_server_ctx(sslsocket this)
{
    int verifyflags = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    int options = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_SINGLE_DH_USE;

    assert(this != NULL);
    assert(this->ctx == NULL);

    this->ctx = SSL_CTX_new(TLSv1_server_method());
    if (this->ctx == NULL)
        goto err;

    // Tell SSL where to find trusted CA certificates
    if (SSL_CTX_load_verify_locations(this->ctx, CAFILE, CADIR) != 1)
        goto err;

    if (SSL_CTX_set_default_verify_paths(this->ctx) != 1)
        goto err;

    // Load CERTFILE from disk
    if (SSL_CTX_use_certificate_chain_file(this->ctx, CERTFILE) != 1)
        goto err;

    // Load Private Key from disk
    if (SSL_CTX_use_PrivateKey_file(this->ctx, CERTFILE, SSL_FILETYPE_PEM) != 1)
        goto err;

    SSL_CTX_set_verify(this->ctx, verifyflags, verify_callback);
    SSL_CTX_set_verify_depth(this->ctx, 4);
    SSL_CTX_set_options(this->ctx, options);

    SSL_CTX_set_tmp_dh_callback(this->ctx, tmp_dh_callback);
    if (SSL_CTX_set_cipher_list(this->ctx, CIPHER_LIST) != 1)
        goto err;

    return success;

err:
    // An error message is (probably) available on SSL's error stack.
    // Pop it and set errno using fail().
    // For now, dump all errors on stderr.
    ERR_print_errors_fp(stderr);

    if (this->ctx != NULL) {
        SSL_CTX_free(this->ctx);
        this->ctx = NULL;
    }

    return failure;
}

// Sets the socket options we want on the main socket
// Suitable for server sockets only.
// SSLTODO: Use an SSL way of setting SO_REUSEADDR
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
// SSLTODO: Polling must change, I guess
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

ssize_t sslsocket_read(sslsocket this, char *dest, size_t count, int timeout, int nretries)
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

// SSLTODO: We need to initialize the _server_, that'll create an SSL_CTX
// SSLTODO: object. We use that object to create a BIO object. Then we accept
// SSLTODO: connections to the BIO, and create an SSL object using
// SSLTODO: SSL_new(), SSL_set_accept_state(), SSL_set_bio(). When all
// SSLTODO: that shit's done, we can call SSL_accept(), and do
// SSLTODO: post connection checks on the server side too. (highly optional)
// SSLTODO:
// SSLTODO: The example programs are good. Go with them
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

    if (!setup_server_ctx(this)) {
        sslsocket_close(this);
        return NULL;
    }

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
    this->ssl = SSL_new(this->ctx);
    if (this->ssl == NULL)
        goto err;

    SSL_set_bio(this->ssl, this->bio, this->bio);
    rc = SSL_connect(this->ssl);
    if (rc <= 0) {
        fprintf(stderr, "SSL_connect failed\n");
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

sslsocket sslsocket_accept(sslsocket this, struct sockaddr *addr, socklen_t *addrsize)
{
    sslsocket new;
    int rc;

    (void)addr;
    (void)addrsize;

    assert(this != NULL);
    assert(addr != NULL);
    assert(addrsize != NULL);

    rc = BIO_do_accept(this->bio);
    if (rc <= 0)
        return NULL;

    if ((new = malloc(sizeof *new)) == NULL) {
        BIO_free(BIO_pop(this->bio)); // Just guessing here...
        return NULL;
    }

    new->bio = BIO_pop(this->bio);
    new->ssl = SSL_new(this->ctx);
    if (new->ssl == NULL) {
        ssl_die(new);
        // Meh.
        return NULL; // Big leak, I know
    }

    SSL_set_accept_state(new->ssl);
    SSL_set_bio(new->ssl, new->bio, new->bio);

    return new;
}
