/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <regex.h>
#include <poll.h>   // for POLLIN

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>

#include <meta_pool.h>
#include <meta_atomic.h>
#include <threadpool.h>
#include <tcp_client.h>
#include <connection.h>
#include <gensocket.h>
#include <cstring.h>

/*
 * Implementation of the TCP client ADT.
 */
struct tcp_client_tag {
    cstring host;
    connection conn;
    membuf readbuf;
    membuf writebuf;

    SSL_CTX *context;
    int timeout_reads, timeout_writes, nretries_read, nretries_write;
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
static SSL_CTX* create_client_context(void)
{
    const SSL_METHOD* method = SSLv23_method();
    if (method == NULL)
        die("Could not get SSL METHOD pointer\n");

    SSL_CTX *ctx = SSL_CTX_new(method);
    if (ctx == NULL)
        die("Unable to create ssl context\n");

    // TODO: This is not good enough
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);
    SSL_CTX_set_verify_depth(ctx, 4);

    const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    SSL_CTX_set_options(ctx, flags);

    long res = SSL_CTX_load_verify_locations(ctx, "random-org-chain.pem", NULL);
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

    if (socktype == SOCKTYPE_SSL 
    && (new->context = create_client_context()) == NULL)
        die("Could not create ssl client context\n");

    if ((new->readbuf = membuf_new(10 * 1024)) == NULL)
        goto memerr;

    if ((new->writebuf = membuf_new(10 * 1024)) == NULL)
        goto memerr;

    // Some default timeout and retry values. Later,
    // change connection_new() to not accept these. Instead,
    // use set/get-functions.
    new->timeout_reads = new->timeout_writes = 1000;
    new->nretries_read = new->nretries_write = 5;

    new->conn = connection_new(socktype, new->timeout_reads, new->timeout_writes,
        new->nretries_read, new->nretries_write, new->context);
    if (new->conn == NULL)
        goto memerr;

    connection_assign_read_buffer(new->conn, new->readbuf);
    connection_assign_write_buffer(new->conn, new->writebuf);

    return new;

memerr:
    membuf_free(new->readbuf);
    membuf_free(new->writebuf);
    connection_free(new->conn);
    SSL_CTX_free(new->context);
    free(new);
    return NULL;
}

void tcp_client_free(tcp_client this)
{
    if (this == NULL)
        return;

    membuf_free(this->readbuf);
    membuf_free(this->writebuf);
    connection_free(this->conn);

    SSL_CTX_free(this->context);
    free(this);
}

status_t tcp_client_connect(tcp_client this, const char *host, int port)
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
    this->nretries_read = count;;
}

void tcp_client_set_retries_write(tcp_client this, int count)
{
    assert(this != NULL);
    this->nretries_write = count;
}

status_t tcp_client_close(tcp_client p)
{
    assert(p != NULL);
    return connection_close(p->conn);
}

