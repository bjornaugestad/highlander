/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <assert.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <connection.h>
#include <gensocket.h>
#include <cstring.h>

#include <tcp_client.h>

#define CIPHER_LIST "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"

struct tcp_client_tag {
    cstring host;

    int socktype;

    // The ciphers member has a default value. The rest are NULL
    cstring rootcert, private_key, ciphers, cadir;
    SSL_CTX *context;

    int timeout_reads, timeout_writes, nretries_read, nretries_write;
    size_t readbuf_size, writebuf_size;

    connection conn;
    membuf readbuf;
    membuf writebuf;
};

// Copied from sample program. (and from tcp_server.c)
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

// Initialize stuff for SSL context
// Beware: We may want only one context per process
static SSL_CTX* create_client_context(void)
{
    const SSL_METHOD* method = SSLv23_method();
    if (method == NULL)
        die("Could not get SSL METHOD pointer\n");

    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL)
        die("Unable to create ssl context\n");

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);
    SSL_CTX_set_verify_depth(ctx, 4);

    const uint64_t flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    SSL_CTX_set_options(ctx, flags);

    // TODO boa@20221125: This can't be hardcoded
    long res = SSL_CTX_load_verify_locations(ctx, "/etc/pki/tls/certs/ca-bundle.trust.crt", NULL);
    if (res != 1)
        die("Could not load verify locations\n");

    return ctx;
}

// It's probably overkill to have one context per connection.
// Then again, we need at least one context and we most likely
// won't have that many client objects in the same client program.
tcp_client tcp_client_new(int socktype)
{
    tcp_client new;

    if ((new = calloc(1, sizeof *new)) == NULL)
        return NULL;

    if ((new->ciphers = cstring_dup(CIPHER_LIST)) == NULL)
        goto memerr;

    if (socktype == SOCKTYPE_SSL 
    && (new->context = create_client_context()) == NULL)
        die("Could not create ssl client context\n");

    new->readbuf_size = new->writebuf_size = 10 * 1024;

    if ((new->readbuf = membuf_new(new->readbuf_size)) == NULL)
        goto memerr;

    if ((new->writebuf = membuf_new(new->writebuf_size)) == NULL)
        goto memerr;

    // Some default timeout and retry values.
    new->timeout_reads = new->timeout_writes = 1000;
    new->nretries_read = new->nretries_write = 5;

    new->conn = connection_new(socktype, new->timeout_reads, new->timeout_writes,
        new->nretries_read, new->nretries_write, new->context);
    if (new->conn == NULL)
        goto memerr;

    connection_assign_read_buffer(new->conn, new->readbuf);
    connection_assign_write_buffer(new->conn, new->writebuf);
    new->socktype = socktype;

    return new;

memerr:
    tcp_client_free(new);
    return NULL;
}

void tcp_client_free(tcp_client this)
{
    if (this == NULL)
        return;

    membuf_free(this->readbuf);
    membuf_free(this->writebuf);
    connection_free(this->conn);

    if (this->socktype == SOCKTYPE_SSL) {
        if (this->context != NULL)
            SSL_CTX_free(this->context);
    }

    cstring_free(this->rootcert);
    cstring_free(this->private_key);
    cstring_free(this->ciphers);
    cstring_free(this->cadir);

    free(this);
}

status_t tcp_client_connect(tcp_client this, const char *host, uint16_t port)
{
    assert(this != NULL);
    assert(host != NULL);

    return connection_connect(this->conn, host, port);
}

connection tcp_client_connection(tcp_client p)
{
    assert(p != NULL);
    assert(p->conn != NULL);

    return p->conn;
}

int tcp_client_get_timeout_write(tcp_client this)
{
    assert(this != NULL);
    return this->timeout_writes;
}

int tcp_client_get_timeout_read(tcp_client this)
{
    assert(this != NULL);
    return this->timeout_reads;
}

void tcp_client_set_timeout_write(tcp_client this, int millisec)
{
    assert(this != NULL);
    this->timeout_writes = millisec;
}

void tcp_client_set_timeout_read(tcp_client this, int millisec)
{
    assert(this != NULL);
    this->timeout_reads = millisec;
}

void tcp_client_set_retries_read(tcp_client this, int count)
{
    assert(this != NULL);
    this->nretries_read = count;
}

void tcp_client_set_retries_write(tcp_client this, int count)
{
    assert(this != NULL);
    this->nretries_write = count;
}

int tcp_client_get_retries_write(tcp_client this)
{
    assert(this != NULL);
    return this->nretries_write;
}

int tcp_client_get_retries_read(tcp_client this)
{
    assert(this != NULL);
    return this->nretries_read;
}

status_t tcp_client_close(tcp_client p)
{
    assert(p != NULL);
    return connection_close(p->conn);
}

status_t tcp_client_set_rootcert(tcp_client this, const char *path)
{
    assert(this != NULL);
    assert(path != NULL);
    assert(strlen(path) > 0);

    // alloc if first time called.
    if (this->rootcert == NULL) {
        this->rootcert = cstring_new();
        if (this->rootcert == NULL)
            return failure;
    }

    return cstring_set(this->rootcert, path);
}

status_t tcp_client_set_private_key(tcp_client this, const char *path)
{
    assert(this != NULL);
    assert(path != NULL);
    assert(strlen(path) > 0);

    // alloc if first time called.
    if (this->private_key == NULL) {
        this->private_key = cstring_new();
        if (this->private_key == NULL)
            return failure;
    }

    return cstring_set(this->private_key, path);
}

status_t tcp_client_set_ciphers(tcp_client this, const char *ciphers)
{
    assert(this != NULL);
    assert(ciphers != NULL);
    assert(this->ciphers != NULL);
    assert(strlen(ciphers) > 0);

    return cstring_set(this->ciphers, ciphers);
}

status_t tcp_client_set_ca_directory(tcp_client this, const char *path)
{
    assert(this != NULL);
    assert(path != NULL);
    assert(strlen(path) > 0);

    // alloc if first time called.
    if (this->cadir == NULL) {
        this->cadir = cstring_new();
        if (this->cadir == NULL)
            return failure;
    }

    return cstring_set(this->cadir, path);
}

void tcp_client_set_readbuf_size(tcp_client p, size_t size)
{
    assert(p != NULL);
    assert(size != 0);

    p->readbuf_size = size;
}

void tcp_client_set_writebuf_size(tcp_client p, size_t size)
{
    assert(p != NULL);
    assert(size != 0);

    p->writebuf_size = size;
}


#ifdef CHECK_TCP_CLIENT

int main(void)
{
    tcp_client p = tcp_client_new(SOCKTYPE_TCP);
    if (p == NULL)
        return 77;

    tcp_client_set_timeout_write(p, 5);
    if (tcp_client_get_timeout_write(p) != 5)
        return 77;

    tcp_client_set_timeout_read(p, 5);
    if (tcp_client_get_timeout_read(p) != 5)
        return 77;

    tcp_client_set_retries_write(p, 5);
    if (tcp_client_get_retries_write(p) != 5)
        return 77;

    tcp_client_set_retries_read(p, 5);
    if (tcp_client_get_retries_read(p) != 5)
        return 77;

    tcp_client_free(p);
    return 0;
}
#endif
