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

#include <miscssl.h>
#include <tcp_client.h>

#define CIPHER_LIST "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256"
struct tcp_client_tag {
    int socktype;
    cstring host;

    // SSL stuff. Messes with regular semantics We need to create the
    // context object, configure it, and then use it. Right now we have no
    // config-hook and "create" hardcodes shit. We should config the context
    // between tcp_client_new() and tcp_client_connect(). That means that
    // our current setter-functions must actually do stuff.
    //    status_t tcp_client_set_rootcert(tcp_client p, const char *path);
    //    status_t tcp_client_set_private_key(tcp_client p, const char *path);
    //    status_t tcp_client_set_ciphers(tcp_client p, const char *ciphers);
    //    status_t tcp_client_set_ca_directory(tcp_client p, const char *path);
    //
    // The ciphers member has a default value. The rest are NULL
    cstring rootcert, private_key, ciphers, cadir;
    SSL_CTX *tc_context;

    unsigned timeout_reads, timeout_writes, nretries_read, nretries_write;
    size_t readbuf_size, writebuf_size;

    connection tc_conn;
    membuf readbuf;
    membuf writebuf;
};

#if 1
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
#endif

// Initialize stuff for SSL context
// Beware: We may want only one context per process
static SSL_CTX* create_client_context(void)
{
    const SSL_METHOD* method = TLS_client_method();
    if (method == NULL)
        die("Could not get SSL METHOD pointer\n");

    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL)
        die("Unable to create ssl context\n");

    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    SSL_CTX_set_default_verify_paths(ctx);

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);
    SSL_CTX_set_verify_depth(ctx, 4);

    const uint64_t flags = SSL_OP_IGNORE_UNEXPECTED_EOF | SSL_OP_NO_COMPRESSION;
    SSL_CTX_set_options(ctx, flags);

    // TODO boa@20221125: This can't be hardcoded
    long res = SSL_CTX_load_verify_locations(ctx, "/etc/pki/tls/certs/ca-bundle.trust.crt", NULL);
    if (res != 1)
        die("Could not load verify locations\n");

    return ctx;
}

tcp_client tcp_client_new(int socktype)
{
    tcp_client new;

    if ((new = calloc(1, sizeof *new)) == NULL)
        return NULL;

    // save it so setters know if SSL or not
    new->socktype = socktype;

    if ((new->ciphers = cstring_dup(CIPHER_LIST)) == NULL)
        goto memerr;

    if (socktype == SOCKTYPE_SSL
    && (new->tc_context = create_client_context()) == NULL)
        die("Could not create ssl client context\n");

    new->readbuf_size = new->writebuf_size = 10 * 1024;
    if ((new->readbuf = membuf_new(new->readbuf_size)) == NULL)
        goto memerr;

    if ((new->writebuf = membuf_new(new->writebuf_size)) == NULL)
        goto memerr;

    // Some default timeout and retry values.
    new->timeout_reads = new->timeout_writes = 1000;
    new->nretries_read = new->nretries_write = 5;

    new->tc_conn = connection_new(socktype, new->timeout_reads, new->timeout_writes,
        new->nretries_read, new->nretries_write, new->tc_context);
    if (new->tc_conn == NULL)
        goto memerr;

    connection_assign_read_buffer(new->tc_conn, new->readbuf);
    connection_assign_write_buffer(new->tc_conn, new->writebuf);

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
    connection_free(this->tc_conn);

    if (this->tc_context != NULL) {
        SSL_CTX_free(this->tc_context);
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

    return connection_connect(this->tc_conn, host, port);
}

connection tcp_client_connection(tcp_client p)
{
    assert(p != NULL);
    assert(p->tc_conn != NULL);

    return p->tc_conn;
}

unsigned tcp_client_get_timeout_write(tcp_client this)
{
    assert(this != NULL);
    return this->timeout_writes;
}

unsigned tcp_client_get_timeout_read(tcp_client this)
{
    assert(this != NULL);
    return this->timeout_reads;
}

void tcp_client_set_timeout_write(tcp_client this, unsigned millisec)
{
    assert(this != NULL);
    this->timeout_writes = millisec;
}

void tcp_client_set_timeout_read(tcp_client this, unsigned millisec)
{
    assert(this != NULL);
    this->timeout_reads = millisec;
}

void tcp_client_set_retries_read(tcp_client this, unsigned count)
{
    assert(this != NULL);
    this->nretries_read = count;
}

void tcp_client_set_retries_write(tcp_client this, unsigned count)
{
    assert(this != NULL);
    this->nretries_write = count;
}

unsigned tcp_client_get_retries_write(tcp_client this)
{
    assert(this != NULL);
    return this->nretries_write;
}

unsigned tcp_client_get_retries_read(tcp_client this)
{
    assert(this != NULL);
    return this->nretries_read;
}

status_t tcp_client_close(tcp_client p)
{
    assert(p != NULL);
    return connection_close(p->tc_conn);
}

status_t tcp_client_set_rootcert(tcp_client this, const char *path)
{
    assert(this != NULL);
    assert(path != NULL);
    assert(strlen(path) > 0);
    assert(this->socktype == SOCKTYPE_SSL);
    assert(this->tc_context != NULL);

    // alloc if first time called.
    if (this->rootcert == NULL) {
        this->rootcert = cstring_new();
        if (this->rootcert == NULL)
            return failure;
    }

    if (!cstring_set(this->rootcert, path))
        return failure;

    // configure the SSL_CTX object.
    int res = SSL_CTX_load_verify_locations(this->tc_context, c_str(this->rootcert), NULL);
    if (!res) {
        unsigned long e = ERR_get_error();
        fprintf(stderr, "%s(): err %s\n", __func__, ERR_reason_error_string(e));
        ERR_print_errors_fp(stderr);
    }
    return res ? success : failure;
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

static inline int test_tcp_client(void)
{
    tcp_client p = tcp_client_new(SOCKTYPE_TCP);
    if (p == NULL)
        return 77;

    tcp_client_set_timeout_write(p, 5);
    if (tcp_client_get_timeout_write(p) != 5)
        goto err;

    tcp_client_set_timeout_read(p, 5);
    if (tcp_client_get_timeout_read(p) != 5)
        goto err;

    tcp_client_set_retries_write(p, 5);
    if (tcp_client_get_retries_write(p) != 5)
        goto err;

    tcp_client_set_retries_read(p, 5);
    if (tcp_client_get_retries_read(p) != 5)
        goto err;

    // We're good to go, but where to?
    status_t rc = tcp_client_connect(p, "www.random.org", 80);
    if (rc != success)
        goto err;

    rc = tcp_client_close(p);
    if (rc != success)
        goto err;

    tcp_client_free(p);
    return 0;

err:
    fprintf(stderr, "%s() failed\n", __func__);
    if (p)
        tcp_client_free(p);
    return 77;
}

static inline int test_ssl_client(void)
{
    if (!openssl_init())
        return 77;

    tcp_client p = tcp_client_new(SOCKTYPE_SSL);
    if (p == NULL)
        return 77;

    // Set certs. We assume we're in project root dir.
    status_t rc = tcp_client_set_rootcert(p, "pki/root/root.crt");
    if (rc != success)
        goto err;

    rc = tcp_client_connect(p, "server.local", 3000);
    if (rc != success)
        goto err;

    rc = tcp_client_close(p);
    if (rc != success)
        goto err;

    tcp_client_free(p);
    if (!openssl_exit())
        goto err;
    return 0;

err:
    fprintf(stderr, "%s() failed\n", __func__);
    if (p)
        tcp_client_free(p);

    if (!openssl_exit())
        return 77; // It's error anyways
    return 77;
}


int main(void)
{
    int rc = test_tcp_client();
    rc += test_ssl_client();
    return rc;
}
#endif
