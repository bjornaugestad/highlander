/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <regex.h>


#include <meta_pool.h>
#include <meta_atomic.h>
#include <threadpool.h>
#include <tcp_server.h>
#include <connection.h>
#include <meta_socket.h>

/**
 * Implementation of the TCP server ADT.
 */
struct tcp_server_tag {
	/* Hostname, for gethostbyaddr */
	char* host;

	/* Port to listen to */
	int port;

	/* timeout in seconds */
	int timeout_reads;
	int timeout_writes;
	int timeout_accepts;

	/* How many times should we try to read/write
	 * before we disconnect? */
	int retries_reads;
	int retries_writes;

	/* The size of the connections read/write buffers */
	size_t readbuf_size, writebuf_size;

	/* Function to call when a new connection is accepted */
	void* (*service_func)(void* arg);
	void* service_arg;

	/* The file descriptor we accept connections from.  */
	meta_socket sock;

	/* The work queue */
	threadpool queue;

	size_t worker_threads;
	size_t queue_size;
	int block_when_full;

	/* Pool of connection objects. Allocated and initiated in
	 * tcp_server_init(), freed in tcp_server_free().
	 * Accessed by tcp_server_get_connection() and
	 * tcp_server_recycle_connection(). Size equals
	 * the # of queue entries + # of worker threads + 1 as
	 * each entry consumes one connection.
	 * The +1 is in case the queue is full.
	 */
	pool connections;

	/* We now have security :-) . The caller can specify
	 * which client that should be able to connect.
	 * We precompile a regexp pattern and store it here
	 * for fast verification of access. */
	regex_t allowed_clients;
	int pattern_compiled;

	/* The pool of read/write buffers */
	pool read_buffers;
	pool write_buffers;

	/* Our shutdown flag */
	int shutting_down;

    /* We want to support UNIX Domain Protocol type sockets (AF_UNIX).
     * This member is by default set to 0, which means AF_INET. If it's
     * set to 1, then hostname is pathname used in sun_path/bind().
     * Use the tcp_server_set_unix_socket() function to enable unix sockets.
     */
    int unix_socket;

	/* Our performance counters */
	atomic_ulong sum_poll_intr; /* # of times poll() returned EINTR */
	atomic_ulong sum_poll_again; /* # of times poll() returned EAGAIN */
	atomic_ulong sum_accept_failed;
	atomic_ulong sum_denied_clients;

};

/* Local helper */
static int client_can_connect(tcp_server srv, struct sockaddr_in* addr);

tcp_server tcp_server_new(void)
{
	tcp_server p;

	if ((p = calloc(1, sizeof *p)) != NULL) {
		/* Some defaults */
		p->timeout_reads = 5000;
		p->timeout_writes = 1000;
		p->timeout_accepts = 800;
		p->retries_reads = 0;
		p->retries_writes = 10;
		p->block_when_full = 0;

		p->queue_size = 100;
		p->worker_threads = 10;
		p->port = 2000;
		p->sock = NULL;
		p->pattern_compiled = 0;
		p->host = NULL;

		p->queue = NULL;
		p->connections = NULL;
		p->readbuf_size =  (1024 * 4);
		p->writebuf_size = (1024 *64);
		p->read_buffers = NULL;
		p->write_buffers = NULL;
		p->shutting_down = 0;
        p->unix_socket = 0;

		atomic_ulong_init(&p->sum_poll_intr);
		atomic_ulong_init(&p->sum_poll_again);
		atomic_ulong_init(&p->sum_accept_failed);
		atomic_ulong_init(&p->sum_denied_clients);
	}

	return p;
}

void tcp_server_free(tcp_server srv)
{
	if (srv != NULL) {
		/* Terminate the session */
		if (srv->queue != NULL) {
			threadpool_destroy(srv->queue, 1);
			srv->queue = NULL;
		}

		pool_free(srv->connections, (dtor)connection_free);
		pool_free(srv->read_buffers, (dtor)membuf_free);
		pool_free(srv->write_buffers, (dtor)membuf_free);

		mem_free(srv->host);

		/* Free the regex struct */
		if (srv->pattern_compiled) {
			regfree(&srv->allowed_clients);
			srv->pattern_compiled = 0;
		}

		atomic_ulong_destroy(&srv->sum_poll_intr);
		atomic_ulong_destroy(&srv->sum_poll_again);
		atomic_ulong_destroy(&srv->sum_accept_failed);
		atomic_ulong_destroy(&srv->sum_denied_clients);
		mem_free(srv);
	}
}

int tcp_server_init(tcp_server srv)
{
	size_t i, conncount, bufcount;

	assert(srv != NULL);

	/* Don't overwrite existing buffers */
	assert(srv->queue == NULL);
	assert(srv->connections == NULL);
	assert(srv->read_buffers == NULL);
	assert(srv->write_buffers == NULL);

	if ((srv->queue = threadpool_new(srv->worker_threads, srv->queue_size, srv->block_when_full)) == NULL)
		goto err;

	/*
	 * Every running worker thread use one connection.
	 * Every queue entry use one connection
	 * One extra is needed for the current connection
	 */
	conncount = srv->queue_size + srv->worker_threads + 1;
	if ((srv->connections = pool_new(conncount)) == NULL)
		goto err;

	for (i = 0; i < conncount; i++) {
		connection c = connection_new(
			srv->timeout_reads,
			srv->timeout_writes,
			srv->retries_reads,
			srv->retries_writes,
			srv->service_arg);

		if (c != NULL)
			pool_add(srv->connections, c);
		else
			goto err;
	}

	/* Only worker threads can use read/write buffers */
	bufcount = srv->worker_threads;
	if ((srv->read_buffers = pool_new(bufcount)) == NULL
	|| (srv->write_buffers = pool_new(bufcount)) == NULL)
		goto err;

	for (i = 0; i < bufcount; i++) {
		membuf rb, wb;

		if ((rb = membuf_new(srv->readbuf_size)) == NULL
		||  (wb = membuf_new(srv->writebuf_size)) == NULL)
			goto err;

		pool_add(srv->read_buffers, rb);
		pool_add(srv->write_buffers, wb);
	}

	return 1;


err:
	/* Free all memory allocated by this function and then return 0 */
	threadpool_destroy(srv->queue, 0);
	pool_free(srv->connections, (dtor)connection_free);
	pool_free(srv->read_buffers, NULL);
	pool_free(srv->write_buffers, NULL);

	srv->queue = NULL;
	srv->connections = NULL;
	srv->read_buffers = NULL;
	srv->write_buffers = NULL;
	return 0;
}

void tcp_server_set_unix_socket(tcp_server s)
{
    assert(s != NULL);
    s->unix_socket = 1;
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

int tcp_server_allow_clients(tcp_server srv, const char* filter)
{
	int err, flags = REG_NOSUB;

	assert(srv != NULL);
	assert(filter != NULL);
	assert(strlen(filter) > 0);

	tcp_server_clear_client_filter(srv);

	if ((err = regcomp(&srv->allowed_clients, filter, flags)) != 0) {
		errno = err;
		return 0;
	}
	else {
		srv->pattern_compiled = 1;
		return 1;
	}
}

void tcp_server_clear_client_filter(tcp_server srv)
{
	assert(srv != NULL);

	if (srv->pattern_compiled) {
		regfree(&srv->allowed_clients);
		srv->pattern_compiled = 0;
	}
}

static void
tcp_server_recycle_connection(void* vse, void* vconn)
{
	membuf rb, wb;

	tcp_server srv = vse;
	connection conn = vconn;

	assert(srv != NULL);
	assert(conn != NULL);

	rb = connection_reclaim_read_buffer(conn);
	wb = connection_reclaim_write_buffer(conn);

	membuf_reset(rb);
	membuf_reset(wb);
	pool_recycle(srv->read_buffers, rb);
	pool_recycle(srv->write_buffers, wb);

	connection_recycle(conn);
	pool_recycle(srv->connections, conn);
}

static void
assign_rw_buffers(void* vse, void* vconn)
{
	membuf rb, wb;

	tcp_server srv;
	connection conn;

	srv = vse;
	conn = vconn;
	assert(srv != NULL);
	assert(srv->read_buffers != NULL);
	assert(srv->write_buffers != NULL);
	assert(conn != NULL);

	rb = pool_get(srv->read_buffers);
	wb = pool_get(srv->write_buffers);

	connection_assign_read_buffer(conn, rb);
	connection_assign_write_buffer(conn, wb);
}

static connection
tcp_server_get_connection(tcp_server srv)
{
	connection conn;

	assert(NULL != srv);
	assert(NULL != srv->connections);

	conn = pool_get(srv->connections);
	assert(NULL != conn);

	return conn;
}

static int accept_new_connections(tcp_server srv, meta_socket sock)
{
	int rc;
	meta_socket newsock;
	socklen_t cbAddr;
	struct sockaddr_in addr;
	connection pconn;

	assert(NULL != srv);
	assert(sock != NULL);

	/* Make the socket non-blocking so that accept() won't block */
	if (!sock_set_nonblock(sock))
		return 0;

	while (!srv->shutting_down) {
		if (!wait_for_data(sock, srv->timeout_accepts)) {
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
				atomic_ulong_inc(&srv->sum_poll_intr);
				continue;
			}
			else if(errno == EAGAIN)  {
				atomic_ulong_inc(&srv->sum_poll_again);
				continue;
			}
			else
				return 0;
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
		cbAddr = sizeof(addr);
		newsock = sock_accept(sock, (struct sockaddr*)&addr, &cbAddr);
		if (newsock == NULL) {
			switch (errno) {
				/*
				 * NOTE: EPROTO is not defined for freebsd, and Stevens
				 * says, in UNP, vol. 1, page 424 that EPROTO should
				 * be ignored. Hmm...
				 */
#ifdef EPROTO
				case EPROTO:
#endif

				/*
				 * NOTE: ENONET does not exist under freebsd, and is
				 * not even mentioned in UNP1. Alan Cox refers to
				 * RFC1122 in a patch posted to news...
				 */
#ifdef ENONET
				case ENONET:
#endif

				/*
				 * AIX specific stuff. Nmap causes accept to return
				 * ENOTCONN, but oddly enough only on port 80.
				 * Let's see if a retry helps.
				 */
				case ENOTCONN:

				case EAGAIN:
				case ENETDOWN:
				case ENOPROTOOPT:
				case EHOSTDOWN:
				case EHOSTUNREACH:
				case EOPNOTSUPP:
				case ENETUNREACH:
					atomic_ulong_inc(&srv->sum_accept_failed);
					continue;

				default:
					return errno;
			}
		}

		/*
		 * Now clear the NONBLOCK flag from the new socket .
		 * According to socket(7), the O_NONBLOCK flag is inherited
		 * through an accept(), but this is NOT correct
		 * in my Linux kernel (2.2.14-5).
		 * According to accept(2) it is not inherited.
		 * I keep the code here anyway...
		 */
		if (!sock_clear_nonblock(newsock))
			return 0;

		/* Check if the client is permitted to connect or not. */
		if (!client_can_connect(srv, &addr)) {
			sock_close(newsock);
			atomic_ulong_inc(&srv->sum_denied_clients);
			continue;
		}

 		/* Get a new, per-connection, struct containing data
 		 * unique to this connection.tcp_server_get_connection()
		 * never returns NULL as enough connection resources has
		 * been allocated already.
 		 */
 		pconn = tcp_server_get_connection(srv);

		/* Start a thread to handle the connection with this client. */
		connection_set_params(pconn, newsock, &addr);

		rc = threadpool_add_work(
			srv->queue,
			assign_rw_buffers,
			srv,
			srv->service_func,
			pconn,
			tcp_server_recycle_connection,
			srv);


		if (!rc) {
			/* Could not add work to the queue */
			/*
			 *  NOTE: The proper HTTP response is:
			 *	503 Service Unavailable
			 *  but tcp_server does not know about HTTP.
			 *  What do we do, add a callback error handler
			 *  or something else? It is, according to
			 *  rfc2616, $10.5.4, OK just to ignore the request,
			 *  but hardly the most userfriendly way of doing it...
			 *  Anyway, if we choose to handle this,
			 *	a) will that create even more overload?
			 *	b) What do we do with the data(if any) that
			 *	the client tries to send us? Can we just
			 *	'dump' a 503 on the socket and then close it?
			 */

			/* NOTE: At this point the connection has not been assigned
			 * any read/write buffers. We therefore cannot close
			 * the connection using connection_close(), but must instead
			 * just discard the connection.
             * UPDATE 20080102: When testing on Snoopy the code
             * breaks in recycle_connection, a function too stupid
             * to know that no read/write buffers has been assigned.
             * Rework this. boa
			 */
			connection_discard(pconn);
			tcp_server_recycle_connection(srv, pconn);
		}
	}

	return 1; /* Shutdown was requested */
}

int tcp_server_get_root_resources(tcp_server srv)
{
	srv->sock = create_server_socket(srv->unix_socket, srv->host, srv->port);
	if (srv->sock == NULL)
		return 0;
	else
		return 1;
}

int tcp_server_start(tcp_server srv)
{
	int rc;

	assert(NULL != srv);

	if (!accept_new_connections(srv, srv->sock)) {
		sock_close(srv->sock);
		rc = 0;
	}
	else if(sock_close(srv->sock))
		rc = 0;
	else
		rc = 1;

	srv->sock = NULL;
	return rc;
}

void tcp_server_set_port(tcp_server srv, int port)
{
	assert(NULL != srv);
	srv->port = port;
}

void tcp_server_set_queue_size(tcp_server srv, size_t size)
{
	assert(NULL != srv);
	srv->queue_size = size;
}

void tcp_server_set_block_when_full(tcp_server srv, int block_when_full)
{
	assert(NULL != srv);
	srv->block_when_full = block_when_full;
}

void tcp_server_set_worker_threads(tcp_server srv, size_t count)
{
	assert(NULL != srv);
	srv->worker_threads = count;
}

void tcp_server_set_timeout(tcp_server srv, int reads, int writes, int accepts)
{
	assert(NULL != srv);
	srv->timeout_writes = writes;
	srv->timeout_reads = reads;
	srv->timeout_accepts = accepts;
}

void tcp_server_set_retries(tcp_server srv, int reads, int writes)
{
	assert(NULL != srv);
	srv->retries_writes = writes;
	srv->retries_reads = reads;
}

#if 0
We no longer use this function, as we want callers to call tcp_server_shutdown
instead.
void tcp_server_set_shutdown_function(
	tcp_server srv,
	int(*func)(void*),
	void* arg)
{
	assert(NULL != srv);
	assert(NULL != func);

	srv->shutting_down = func;
	srv->shutdown_arg = arg;
}
#endif

void tcp_server_set_service_function(
	tcp_server srv,
	void* (*func)(void*),
	void* arg)
{
	assert(NULL != srv);
	assert(NULL != func);

	srv->service_func = func;
	srv->service_arg = arg;
}

int tcp_server_set_hostname(tcp_server srv, const char* host)
{
	if (srv->host != NULL)
		mem_free(srv->host);

	if (host == NULL)
		srv->host = NULL;
	else if((srv->host = malloc(strlen(host) + 1)) == NULL)
		return 0;
	else
		strcpy(srv->host, host);

	return 1;
}

/**
 * Checks to see if the client can connect or not.
 * A client can connect if
 * a) The ip is listed in the allowed list.
 * b) The list of allowed clients is empty.
 *
 * This is new stuff, so some notes.
 * a) Allow DNS names or not?
 *    We do not want to be vulnerable to DNS spoofing attacks.
 *    At the same time we want easy configuration.
 *    Safety first, which means that we only match IP for now.
 *    DNS also means that we must do a getpeername(), which is slow.
 *
 * b) If the caller has set us up to do access control, we'll
 *    already have a precompiled regexp available. All we now
 *    have to do is to regexec.
 */
static int client_can_connect(tcp_server srv, struct sockaddr_in* addr)
{
	int vaddr;
#ifdef HAVE_INET_NTOP
	char sz[INET_ADDRSTRLEN + 1];
#else
	char sz[2000 + 1];
#endif

	assert(srv != NULL);
	assert(addr != NULL);

	vaddr = addr->sin_addr.s_addr;
	if (!srv->pattern_compiled) {
		/* No permissions set. Allow all */
		return 1;
	}
	else if(inet_ntop(AF_INET, &vaddr, sz, sizeof(sz)) == NULL) {
		/* Crappy addr or internal error. Deny */
		return 0;
	}
	else if(regexec(&srv->allowed_clients, sz, 0, NULL, 0) == REG_NOMATCH){
		/* Not found in pattern */
		return 0;
	}
	else
		return 1;
}

int tcp_server_start_via_process(process p, tcp_server s)
{
	return process_add_object_to_start(
		p,
		s,
		(int(*)(void*))tcp_server_get_root_resources,
		(int(*)(void*))tcp_server_free_root_resources,
		(int(*)(void*))tcp_server_start,
		(int(*)(void*))tcp_server_shutdown);
}

int tcp_server_free_root_resources(tcp_server s)
{
	/* NOTE: 2005-11-27: Check out why we don't close the socket here. */
	(void)s;
	return 1;
}

int tcp_server_shutting_down(tcp_server srv)
{
	assert(srv != NULL);
	return srv->shutting_down;
}

int tcp_server_shutdown(tcp_server srv)
{
	assert(srv != NULL);
	srv->shutting_down = 1;
	return 1;
}

unsigned long tcp_server_sum_blocked(tcp_server p)
{
	assert(p != NULL);
	return threadpool_sum_blocked(p->queue);
}

unsigned long tcp_server_sum_discarded(tcp_server p)
{
	assert(p != NULL);
	return threadpool_sum_discarded(p->queue);
}

unsigned long tcp_server_sum_added(tcp_server p)
{
	assert(p != NULL);
	return threadpool_sum_added(p->queue);
}

unsigned long tcp_server_sum_poll_intr(tcp_server p)
{
	assert(p != NULL);
	return atomic_ulong_get(&p->sum_poll_intr);
}

unsigned long tcp_server_sum_poll_again(tcp_server p)
{
	assert(p != NULL);
	return atomic_ulong_get(&p->sum_poll_again);
}

unsigned long tcp_server_sum_accept_failed(tcp_server p)
{
	assert(p != NULL);
	return atomic_ulong_get(&p->sum_accept_failed);
}

unsigned long tcp_server_sum_denied_clients(tcp_server p)
{
	assert(p != NULL);
	return atomic_ulong_get(&p->sum_denied_clients);
}

