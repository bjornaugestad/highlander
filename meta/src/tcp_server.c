/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
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
#include <tcp_server.h>
#include <connection.h>
#include <gensocket.h>
#include <cstring.h>

struct tcp_server_tag {
    /* Hostname, for gethostbyaddr */
    cstring host;

    /* Port to listen to */
    int port;

    // SSL or TCP
    int socktype;

    // timeout in milliseconds
    int timeout_reads, timeout_writes, timeout_accepts;

    // How many times should we try to read/write before we disconnect?
    int retries_reads, retries_writes;

    // The size of the connections read/write buffers
    size_t readbuf_size, writebuf_size;

    // Some SSL related properties we need. They're per
    // SSL_CTX, so for us, per tcp_server and highly optional.
    // Regular servers need none of these.
    cstring server_cert, private_key, cadir, cert_chain_file;
    SSL_CTX *ctx;

    // Function to call when a new connection is accepted
    void *(*service_func)(void *arg);
    void *service_arg;

    // The socket we accept connections from.
    socket_t listener;

    /* The work queue */
    threadpool queue;

    size_t nthreads;
    size_t queue_size;
    int block_when_full;

    // Pool of connection objects. Allocated and initiated in tcp_server_init(),
    // freed in tcp_server_free().  Accessed by tcp_server_get_connection() and
    // tcp_server_recycle_connection(). Size equals the # of queue entries
    // + # of worker threads + 1 as each entry consumes one connection.
    // The +1 is in case the queue is full.
    pool connections;

    // The caller can specify which client that should be able to connect.
    // We precompile a regexp pattern and store it here for fast verification.
    regex_t allowed_clients;
    bool pattern_compiled;

    // The pool of read/write buffers. The pools contain instances of membuf ADT
    pool read_buffers;
    pool write_buffers;

    // Our shutdown flag
    int shutting_down;

    /* Our performance counters */
    atomic_ulong sum_poll_intr; /* # of times poll() returned EINTR */
    atomic_ulong sum_poll_again; /* # of times poll() returned EAGAIN */
    atomic_ulong sum_accept_failed;
    atomic_ulong sum_denied_clients;
};

/*
 * Checks to see if the client can connect or not.
 * A client can connect if
 * a) The ip is listed in the allowed list.
 * b) The list of allowed clients is empty.
 *
 * This is new stuff, so some notes.
 * a) Allow DNS names or not?
 *   We do not want to be vulnerable to DNS spoofing attacks.
 *   At the same time we want easy configuration.
 *   Safety first, which means that we only match IP for now.
 *   DNS also means that we must do a getpeername(), which is slow.
 *
 * b) If the caller has set us up to do access control, we'll
 *   already have a precompiled regexp available. All we now
 *   have to do is to regexec.
 */
static bool client_can_connect(tcp_server srv, struct sockaddr_in* addr)
{
    int vaddr;
    char sz[INET_ADDRSTRLEN + 1];

    assert(srv != NULL);
    assert(addr != NULL);

    vaddr = addr->sin_addr.s_addr;
    if (!srv->pattern_compiled) {
        /* No permissions set. Allow all */
        return true;
    }

    if (inet_ntop(AF_INET, &vaddr, sz, sizeof sz) == NULL) {
        /* Crappy addr or internal error. Deny */
        return false;
    }

    if (regexec(&srv->allowed_clients, sz, 0, NULL, 0) == REG_NOMATCH){
        /* Not found in pattern */
        return false;
    }

    return true;
}

tcp_server tcp_server_new(int socktype)
{
    tcp_server new;

    if ((new = calloc(1, sizeof *new)) != NULL) {
        /* Some defaults */
        new->timeout_reads = 5000;
        new->timeout_writes = 1000;
        new->timeout_accepts = 800;
        new->retries_writes = 10;

        new->queue_size = 100;
        new->nthreads = 10;
        new->port = 2000;
        new->socktype = socktype;

        new->pattern_compiled = false;

        new->readbuf_size =  (1024 * 4);
        new->writebuf_size = (1024 *64);
        new->shutting_down = 0;

        atomic_ulong_init(&new->sum_poll_intr);
        atomic_ulong_init(&new->sum_poll_again);
        atomic_ulong_init(&new->sum_accept_failed);
        atomic_ulong_init(&new->sum_denied_clients);
    }

    return new;
}

void tcp_server_free(tcp_server this)
{
    if (this == NULL)
        return;

    /* Terminate the session */
    if (this->queue != NULL) {
        if (!threadpool_destroy(this->queue, 1))
            warning("Unable to destroy thread pool\n");

        this->queue = NULL;
    }

    pool_free(this->connections, (dtor)connection_free);
    pool_free(this->read_buffers, (dtor)membuf_free);
    pool_free(this->write_buffers, (dtor)membuf_free);

    cstring_free(this->host);
    cstring_free(this->server_cert);
    cstring_free(this->private_key);
    cstring_free(this->cert_chain_file);
    cstring_free(this->cadir);

    if (this->socktype == SOCKTYPE_SSL)
        SSL_CTX_free(this->ctx);

    /* Free the regex struct */
    if (this->pattern_compiled) {
        regfree(&this->allowed_clients);
        this->pattern_compiled = false;
    }

    atomic_ulong_destroy(&this->sum_poll_intr);
    atomic_ulong_destroy(&this->sum_poll_again);
    atomic_ulong_destroy(&this->sum_accept_failed);
    atomic_ulong_destroy(&this->sum_denied_clients);
    free(this);
}

status_t tcp_server_init(tcp_server this)
{
    size_t i, count;

    assert(this != NULL);

    /* Don't overwrite existing buffers */
    assert(this->queue == NULL);
    assert(this->connections == NULL);
    assert(this->read_buffers == NULL);
    assert(this->write_buffers == NULL);

    this->queue = threadpool_new(this->nthreads, this->queue_size, this->block_when_full);
    if (this->queue == NULL)
        goto err;

    // Every running worker thread use one connection. Every queue entry use
    // one connection.  One extra is needed for the current connection.
    count = this->queue_size + this->nthreads + 1;
    if ((this->connections = pool_new(count)) == NULL)
        goto err;

    for (i = 0; i < count; i++) {
        connection conn = connection_new(this->socktype, this->timeout_reads, this->timeout_writes,
            this->retries_reads, this->retries_writes, this->service_arg);

        if (conn == NULL)
            goto err;

        pool_add(this->connections, conn);
    }

    /* Only worker threads can use read/write buffers */
    count = this->nthreads;
    if ((this->read_buffers = pool_new(count)) == NULL
    || (this->write_buffers = pool_new(count)) == NULL)
        goto err;

    for (i = 0; i < count; i++) {
        membuf rb, wb;

        if ((rb = membuf_new(this->readbuf_size)) == NULL
        || (wb = membuf_new(this->writebuf_size)) == NULL)
            goto err;

        pool_add(this->read_buffers, rb);
        pool_add(this->write_buffers, wb);
    }

    return success;

err:
    // Free all memory allocated by this function and then return failure.
    if (!threadpool_destroy(this->queue, 0))
        warning("Unable to destroy thread pool\n");

    pool_free(this->connections, (dtor)connection_free);
    pool_free(this->read_buffers, NULL);
    pool_free(this->write_buffers, NULL);

    this->queue = NULL;
    this->connections = NULL;
    this->read_buffers = NULL;
    this->write_buffers = NULL;
    return failure;
}

void tcp_server_set_readbuf_size(tcp_server s, size_t size)
{
    assert(s != NULL);
    assert(size != 0);

    s->readbuf_size = size;
}

void tcp_server_set_writebuf_size(tcp_server s, size_t size)
{
    assert(s != NULL);
    assert(size != 0);

    s->writebuf_size = size;
}

status_t tcp_server_allow_clients(tcp_server this, const char *filter)
{
    int err, flags = REG_NOSUB;

    assert(this != NULL);
    assert(filter != NULL);
    assert(strlen(filter) > 0);

    tcp_server_clear_client_filter(this);

    if ((err = regcomp(&this->allowed_clients, filter, flags)) != 0)
        return fail(err);

    this->pattern_compiled = true;
    return success;
}

void tcp_server_clear_client_filter(tcp_server this)
{
    assert(this != NULL);

    if (this->pattern_compiled) {
        regfree(&this->allowed_clients);
        this->pattern_compiled = false;
    }
}

static void tcp_server_recycle_connection(void *vsrv, void *vconn)
{
    membuf rb, wb;

    tcp_server srv = vsrv;
    connection conn = vconn;

    assert(srv != NULL);
    assert(conn != NULL);

    rb = connection_reclaim_read_buffer(conn);
    wb = connection_reclaim_write_buffer(conn);

    if (rb != NULL) {
        membuf_reset(rb);
        pool_recycle(srv->read_buffers, rb);
    }

    if (wb != NULL) {
        membuf_reset(wb);
        pool_recycle(srv->write_buffers, wb);
    }

    connection_recycle(conn);
    pool_recycle(srv->connections, conn);
}

static status_t assign_rw_buffers(void *vsrv, void *vconn)
{
    membuf rb, wb;

    tcp_server srv;
    connection conn;

    srv = vsrv;
    conn = vconn;
    assert(srv != NULL);
    assert(srv->read_buffers != NULL);
    assert(srv->write_buffers != NULL);
    assert(conn != NULL);

    if (!pool_get(srv->read_buffers, (void **)&rb))
        return failure;

    if (!pool_get(srv->write_buffers, (void **)&wb))
        return failure;

    connection_assign_read_buffer(conn, rb);
    connection_assign_write_buffer(conn, wb);
    return success;
}

static status_t tcp_server_get_connection(tcp_server srv, connection *pconn)
{
    assert(srv != NULL);
    assert(srv->connections != NULL);
    assert(pconn != NULL);

    return pool_get(srv->connections, (void **)pconn);
}

// See "Advanced Programming in the UNIX environment" for a discussion of EINTR,
// select(), SA_RESTART and portability between SVR4 and BSD. Interesting
// chapters are 12.5 and 10.x
//
// if polling returned success, we most likely have a new connection present.
// The connection may have been closed between the poll() and our accept, so the
// non-blocking accept() may return -1. errno will then be EAGAIN | EWOULDBLOCK.
//
// In addition to this, Linux, according to accept(2), will pass any pending
// errors as an error code to accept(). The errors listed in the man page are:
//	ENETDOWN, EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET,
//	EHOSTUNREACH, EOPNOTSUPP and ENETUNREACH.
// These errors should, on Linux, be treated as EAGAIN acc. to the man page.

static status_t accept_new_connections(tcp_server this, socket_t sock)
{
    status_t rc;
    socket_t newsock;
    struct sockaddr_in addr;
    socklen_t addrsize = sizeof addr;
    connection conn;

    assert(this != NULL);
    assert(sock != NULL);

    while (!this->shutting_down) {
        if (!socket_poll_for(sock, this->timeout_accepts, POLLIN)) {
            if (errno == EINTR)
                atomic_ulong_inc(&this->sum_poll_intr);
            else if (errno == EAGAIN)
                atomic_ulong_inc(&this->sum_poll_again);
            else
                return failure;

            continue; // retry
        }

        newsock = socket_accept(sock, this->ctx, (struct sockaddr*)&addr, &addrsize);
        if (newsock == NULL) {
            switch (errno) {
                // EPROTO is not defined for freebsd, and Stevens says, in UNP,
                // vol. 1, page 424 that EPROTO should be ignored.
#ifdef EPROTO
                case EPROTO:
#endif

                // ENONET does not exist under freebsd, and is not even mentioned
                // in UNP1. Alan Cox refers to RFC1122 in a patch posted to news.
#ifdef ENONET
                case ENONET:
#endif

                // AIX specific stuff. Nmap causes accept to return ENOTCONN,
                // but oddly enough only on port 80. Let's see if a retry helps.
                case ENOTCONN:

                case EAGAIN:
                case ENETDOWN:
                case ENOPROTOOPT:
                case EHOSTDOWN:
                case EHOSTUNREACH:
                case EOPNOTSUPP:
                case ENETUNREACH:
                    atomic_ulong_inc(&this->sum_accept_failed);
                    continue;

                default:
                    // We may want to retry here, to avoid DoS attacks
                    // and to survive unknown errno values.
                    continue;
            }
        }

        // Check if the client is permitted to connect or not.
        if (!client_can_connect(this, &addr)) {
            socket_close(newsock);
            atomic_ulong_inc(&this->sum_denied_clients);
            continue;
        }

         // Get a new, per-connection, struct containing data unique to this
         // connection. tcp_server_get_connection() never returns NULL as enough
         // connection resources has been allocated already.
         if (!tcp_server_get_connection(this, &conn)) {
            socket_close(newsock);
            return failure;
         }

        // Start a thread to handle the connection with this client.
        connection_set_params(conn, newsock, &addr);

        rc = threadpool_add_work(this->queue,
            assign_rw_buffers, this,
            this->service_func, conn,
            tcp_server_recycle_connection, this);

        // The queue was full, so we couldn't add the new connection to the 
        // queue. We must therefore close the connection and do some cleanup.
        if (!rc) {
            if (!connection_close(conn))
                warning("Could not flush and close connection");

            tcp_server_recycle_connection(this, conn);
        }
    }

    return success; /* Shutdown was requested */
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

static status_t destroy_server_ctx(tcp_server this)
{
    assert(this != NULL);

    printf("%s(): I'm here\n", __func__);
    if (this->ctx != NULL)
        SSL_CTX_free(this->ctx);

    return success;
}

static status_t setup_server_ctx(tcp_server this)
{
    int verifyflags = 0; // SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    int options = SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE;
    int rc;
    const SSL_METHOD *method;

    assert(this != NULL);
    assert(this->ctx == NULL);
    assert(this->server_cert != NULL);
    assert(this->private_key != NULL);

    method = TLS_server_method();
    if (method == NULL)
        goto err;

    this->ctx = SSL_CTX_new(method);
    if (this->ctx == NULL)
        goto err;

    // Tell SSL where to find trusted CA certificates
    const char *server_cert = c_str(this->server_cert);

    const char *cadir = NULL;
    if (this->cadir != NULL)
        cadir = c_str(this->cadir);
        
    rc = SSL_CTX_load_verify_locations(this->ctx, server_cert, cadir);
    if (rc != 1)
        goto err;

    rc = SSL_CTX_set_default_verify_paths(this->ctx);
    if (rc != 1)
        goto err;

    // Load certfile from disk. chained certs are optional.
    if (this->cert_chain_file != NULL) {
        const char *cert_chain_file = c_str(this->cert_chain_file);
        rc = SSL_CTX_use_certificate_chain_file(this->ctx, cert_chain_file);
    }
    else // Just use server cert directly
        rc = SSL_CTX_use_certificate_file(this->ctx, server_cert, SSL_FILETYPE_PEM);

    if (rc != 1)
        goto err;


    // Private key goes here.
    const char *private_key = c_str(this->private_key);
    rc = SSL_CTX_use_PrivateKey_file(this->ctx, private_key, SSL_FILETYPE_PEM);
    if (rc != 1)
        goto err;

    SSL_CTX_set_verify(this->ctx, verifyflags, verify_callback);
    SSL_CTX_set_verify_depth(this->ctx, 4);
    SSL_CTX_set_options(this->ctx, options);
    SSL_CTX_clear_options(this->ctx, SSL_OP_NO_TLSv1_3);
    SSL_CTX_set_min_proto_version(this->ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(this->ctx, TLS1_3_VERSION);

    return success;

err:
    // An error message is (probably) available on SSL's error stack.
    // Pop it and set errno using fail().
    // For now, dump all errors on stderr.
    fprintf(stderr, "%s(): Some error occurred\n", __func__);
    ERR_print_errors_fp(stderr);

    if (this->ctx != NULL) {
        SSL_CTX_free(this->ctx);
        this->ctx = NULL;
    }

    return failure;
}

status_t tcp_server_get_root_resources(tcp_server this)
{
    const char *hostname = "localhost";

    if (this->host != NULL)
        hostname = c_str(this->host);

    if (this->socktype == SOCKTYPE_SSL && !setup_server_ctx(this))
        return failure;

    this->listener = socket_create_server_socket(this->socktype, hostname, this->port);
    if (this->listener == NULL)
        return failure;

    return success;
}

status_t tcp_server_start(tcp_server this)
{
    status_t rc;

    assert(this != NULL);

    if (!accept_new_connections(this, this->listener)) {
        socket_close(this->listener);
        rc = failure;
    }
    else if (!socket_close(this->listener))
        rc = failure;
    else
        rc = success;

    this->listener = NULL;
    return rc;
}

void tcp_server_set_port(tcp_server this, int port)
{
    assert(this != NULL);
    this->port = port;
}

void tcp_server_set_queue_size(tcp_server this, size_t size)
{
    assert(this != NULL);
    this->queue_size = size;
}

void tcp_server_set_block_when_full(tcp_server this, int block_when_full)
{
    assert(this != NULL);
    this->block_when_full = block_when_full;
}

void tcp_server_set_worker_threads(tcp_server this, size_t count)
{
    assert(this != NULL);
    this->nthreads = count;
}

void tcp_server_set_timeout(tcp_server this, int reads, int writes, int accepts)
{
    assert(this != NULL);

    this->timeout_writes = writes;
    this->timeout_reads = reads;
    this->timeout_accepts = accepts;
}

void tcp_server_set_retries(tcp_server this, int reads, int writes)
{
    assert(this != NULL);
    this->retries_writes = writes;
    this->retries_reads = reads;
}

void tcp_server_set_service_function(
    tcp_server this,
    void *(*func)(void*),
    void *arg)
{
    assert(this != NULL);
    assert(func != NULL);

    this->service_func = func;
    this->service_arg = arg;
}

status_t tcp_server_set_hostname(tcp_server this, const char *host)
{
    assert(this != NULL);

    cstring_free(this->host);

    if (host == NULL) {
        this->host = NULL;
        return success;
    }

    if ((this->host = cstring_dup(host)) == NULL)
        return failure;

    return success;
}

status_t tcp_server_start_via_process(process p, tcp_server s)
{
    return process_add_object_to_start(p, s,
        (status_t(*)(void*))tcp_server_get_root_resources,
        (status_t(*)(void*))tcp_server_free_root_resources,
        (status_t(*)(void*))tcp_server_start,
        (status_t(*)(void*))tcp_server_shutdown);
}

// 20251006: Something's fucked with SSL teardown. Also, are SSL resources
// root resources? Hmm, we should probably move things around a bit,
// probably to a dtor. For testing purposes, we use the (get|free)_root_resources
// until teardown is correct. 
status_t tcp_server_free_root_resources(tcp_server this)
{
    printf("%s(): I'm here\n", __func__);

    /* NOTE: 2005-11-27: Check out why we don't close the socket here. */
    // NOTE: 2017-05-17: Maybe because it's the accept-socket? We should still close it, though
    // NOTE: 2025-10-06: socket_close() now closes *and* frees the generic socket.
    // NOTE: 2025-10-08: Perhaps not destroy server context before closing socket?
    assert(this != NULL);
    status_t rc = success;
    if (this->listener != NULL)
        rc = socket_close(this->listener);

    if (this->socktype == SOCKTYPE_SSL)
        destroy_server_ctx(this);

    return rc;
}

int tcp_server_shutting_down(tcp_server this)
{
    assert(this != NULL);
    return this->shutting_down;
}

status_t tcp_server_shutdown(tcp_server this)
{
    assert(this != NULL);
    this->shutting_down = 1;
    return success;
}

unsigned long tcp_server_sum_blocked(tcp_server this)
{
    assert(this != NULL);
    return threadpool_sum_blocked(this->queue);
}

unsigned long tcp_server_sum_discarded(tcp_server this)
{
    assert(this != NULL);
    return threadpool_sum_discarded(this->queue);
}

unsigned long tcp_server_sum_added(tcp_server this)
{
    assert(this != NULL);
    return threadpool_sum_added(this->queue);
}

unsigned long tcp_server_sum_poll_intr(tcp_server this)
{
    assert(this != NULL);
    return atomic_ulong_get(&this->sum_poll_intr);
}

unsigned long tcp_server_sum_poll_again(tcp_server this)
{
    assert(this != NULL);
    return atomic_ulong_get(&this->sum_poll_again);
}

unsigned long tcp_server_sum_accept_failed(tcp_server this)
{
    assert(this != NULL);
    return atomic_ulong_get(&this->sum_accept_failed);
}

unsigned long tcp_server_sum_denied_clients(tcp_server this)
{
    assert(this != NULL);
    return atomic_ulong_get(&this->sum_denied_clients);
}

status_t tcp_server_set_server_cert(tcp_server this, const char *path)
{
    assert(this != NULL);
    assert(path != NULL);
    assert(strlen(path) > 0);

    // alloc if first time called.
    if (this->server_cert == NULL) {
        this->server_cert = cstring_new();
        if (this->server_cert == NULL)
            return failure;
    }

    return cstring_set(this->server_cert, path);
}

status_t tcp_server_set_private_key(tcp_server this, const char *path)
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

status_t tcp_server_set_cert_chain_file(tcp_server this, const char *path)
{
    assert(this != NULL);
    assert(path != NULL);
    assert(strlen(path) > 0);

    // alloc if first time called.
    if (this->cert_chain_file == NULL) {
        this->cert_chain_file = cstring_new();
        if (this->cert_chain_file == NULL)
            return failure;
    }

    return cstring_set(this->cert_chain_file, path);
}

status_t tcp_server_set_ca_directory(tcp_server this, const char *path)
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

#ifdef CHECK_TCP_SERVER

// How to test the tcp server? A couple of no-brainer tests
// would be to start a server and connect to it. More advanced tests
// could be what?
int main(void)
{
    tcp_server srv = tcp_server_new(SOCKTYPE_TCP);

    tcp_server_free(srv);
    return 0;
}
#endif
