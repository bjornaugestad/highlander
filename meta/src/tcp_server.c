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

#include <meta_pool.h>
#include <meta_atomic.h>
#include <threadpool.h>
#include <tcp_server.h>
#include <connection.h>
#include <gensocket.h>
#include <cstring.h>

/*
 * Implementation of the TCP server ADT.
 */
struct tcp_server_tag {
    /* Hostname, for gethostbyaddr */
    cstring host;

    /* Port to listen to */
    int port;

    // timeout in seconds
    int timeout_reads, timeout_writes, timeout_accepts;

    // How many times should we try to read/write before we disconnect?
    int retries_reads, retries_writes;

    // The size of the connections read/write buffers
    size_t readbuf_size, writebuf_size;

    // Function to call when a new connection is accepted
    void *(*service_func)(void *arg);
    void *service_arg;

    // The socket we accept connections from.
    int socktype;
    socket_t sock;

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

    // The pool of read/write buffers. The pools contain instances of membufs,
    //
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
 *	  We do not want to be vulnerable to DNS spoofing attacks.
 *	  At the same time we want easy configuration.
 *	  Safety first, which means that we only match IP for now.
 *	  DNS also means that we must do a getpeername(), which is slow.
 *
 * b) If the caller has set us up to do access control, we'll
 *	  already have a precompiled regexp available. All we now
 *	  have to do is to regexec.
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

// SSLTODO: We need a per-server SSL_CTX, I guess. Remember that one
// SSLTODO: UNIX process can contain many active tcp_server objects
// SSLTODO: and run many servers from within the same process.
// SSLTODO:
// SSLTODO: Each socket(active connection) will have its own SSL object.
tcp_server tcp_server_new(int socktype)
{
    tcp_server new;

    if ((new = calloc(1, sizeof *new)) != NULL) {
        /* Some defaults */
        new->timeout_reads = 5000;
        new->timeout_writes = 1000;
        new->timeout_accepts = 800;
        new->retries_reads = 0;
        new->retries_writes = 10;
        new->block_when_full = 0;

        new->queue_size = 100;
        new->nthreads = 10;
        new->port = 2000;
        new->socktype = socktype;
        new->sock = NULL;
        new->pattern_compiled = false;
        new->host = NULL;

        new->queue = NULL;
        new->connections = NULL;
        new->readbuf_size =  (1024 * 4);
        new->writebuf_size = (1024 *64);
        new->read_buffers = NULL;
        new->write_buffers = NULL;
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
    if (this != NULL) {
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
        connection conn = connection_new(this->timeout_reads, this->timeout_writes,
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
        ||	(wb = membuf_new(this->writebuf_size)) == NULL)
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

static void
tcp_server_recycle_connection(void *vsrv, void *vconn)
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

static status_t accept_new_connections(tcp_server this, socket_t sock)
{
    status_t rc;
    socket_t newsock;
    socklen_t addrsize;
    struct sockaddr_in addr;
    connection conn;

    assert(this != NULL);
    assert(sock != NULL);

    /* Make the socket non-blocking so that accept() won't block */
    if (!socket_set_nonblock(sock))
        return failure;

    while (!this->shutting_down) {
        if (!socket_poll_for(sock, this->timeout_accepts, POLLIN)) {
            if (errno == EINTR) {
                /* Someone interrupted us, why?
                 * NOTE: This happens when the load is very high
                 * and the number of connections in TIME_WAIT
                 * state is high (800+).
                 *
                 * What do we do, just restart or try to handle some
                 * condition? We just restart for now...
                 *
                 * See "Advanced Programming in the UNIX environment"
                 * for a discussion of EINTR, select(), SA_RESTART
                 * and portability between SVR4 and BSD-based kernels.
                 * Interesting chapters are 12.5 and 10.x
                 */
                atomic_ulong_inc(&this->sum_poll_intr);
                continue;
            }

            if (errno == EAGAIN)  {
                atomic_ulong_inc(&this->sum_poll_again);
                continue;
            }

            return failure;
        }

        /*
         * Now we most likely have a new connection present.
         * The connection may have been closed between the select()
         * above and here, so the non-blocking accept() may return -1.
         * errno will then be EAGAIN | EWOULDBLOCK.
         * In addition to this, Linux, according to accept(2), will
         * pass any pending errors as an error code to accept(). The
         * errors listed in the man page are:
         *	ENETDOWN, EPROTO, ENOPROTOOPT, EHOSTDOWN, ENONET,
         *	EHOSTUNREACH, EOPNOTSUPP and ENETUNREACH.
         * These errors should, on Linux, be treated as EAGAIN acc.
         * to the man page.
         */
        /*
         * NOTE: BSD may require us to set sockaddr.sa_len
         * to sizeof struct sockaddr_XX
         * Linux does not as it doesn't have that struct member.
         */
        addrsize = sizeof addr;
        newsock = socket_accept(sock, (struct sockaddr*)&addr, &addrsize);
        if (newsock == NULL) {
            switch (errno) {
                /* NOTE: EPROTO is not defined for freebsd, and Stevens
                 * says, in UNP, vol. 1, page 424 that EPROTO should
                 * be ignored. Hmm...  */
#ifdef EPROTO
                case EPROTO:
#endif

                /* NOTE: ENONET does not exist under freebsd, and is
                 * not even mentioned in UNP1. Alan Cox refers to
                 * RFC1122 in a patch posted to news... */
#ifdef ENONET
                case ENONET:
#endif

                /* AIX specific stuff. Nmap causes accept to return
                 * ENOTCONN, but oddly enough only on port 80.
                 * Let's see if a retry helps. */
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
                    return failure;
            }
        }

        /* Check if the client is permitted to connect or not. */
        if (!client_can_connect(this, &addr)) {
            socket_close(newsock);
            atomic_ulong_inc(&this->sum_denied_clients);
            continue;
        }

         /* Get a new, per-connection, struct containing data
          * unique to this connection. tcp_server_get_connection()
          * never returns NULL as enough connection resources has
          * been allocated already. */
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

        if (!rc) {
            /* Could not add work to the queue */
            /*
             *	NOTE: The proper HTTP response is "503 Service Unavailable"
             *	but tcp_server does not know about HTTP. What do we do, add a
             *	callback error handler or something else? It is, according to
             *	rfc2616, $10.5.4, OK just to ignore the request, but hardly the
             *	most userfriendly way of doing it.  Anyway, if we choose to
             *	handle this,
             *	a) will that create even more overload?
             *	b) What do we do with the data(if any) that the client tries to
             *	send us? Can we just 'dump' a 503 on the socket and then close it?
             */
            if (!connection_close(conn))
                warning("Could not flush and close connection");

            tcp_server_recycle_connection(this, conn);
        }
    }

    return success; /* Shutdown was requested */
}

status_t tcp_server_get_root_resources(tcp_server this)
{
    const char *hostname = "localhost";

    if (this->host != NULL)
        hostname = c_str(this->host);

    this->sock = socket_create_server_socket(this->socktype, hostname, this->port);
    if (this->sock == NULL)
        return failure;

    return success;
}

status_t tcp_server_start(tcp_server this)
{
    status_t rc;

    assert(this != NULL);

    if (!accept_new_connections(this, this->sock)) {
        socket_close(this->sock);
        rc = failure;
    }
    else if (!socket_close(this->sock))
        rc = failure;
    else
        rc = success;

    this->sock = NULL;
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

    if (this->host != NULL)
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

status_t tcp_server_free_root_resources(tcp_server s)
{
    /* NOTE: 2005-11-27: Check out why we don't close the socket here. */
    // NOTE: 2017-05-17: Maybe because it's the accept-socket? We should still close it, though
    assert(s != NULL);
    if (s->sock != NULL)
        return socket_close(s->sock);
    return success;
}

int tcp_server_shutting_down(tcp_server this)
{
    assert(this != NULL);
    return this->shutting_down;
}

int tcp_server_shutdown(tcp_server this)
{
    assert(this != NULL);
    this->shutting_down = 1;
    return 1;
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
