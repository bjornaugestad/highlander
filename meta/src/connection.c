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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <meta_common.h>
#include <meta_membuf.h>
#include <connection.h>
#include <meta_socket.h>

/*
 * NOTE: Security
 * Black lists:
 * A black list is a list of IP adresses which aren't allowed to
 * connect.
 * Tar pitting:
 * - What is it? Tar pitting is to return data very slowly to a
 * malicious client. That way we bind his resources and slow him
 * down. It can easily be implemented by changing the connection_flush()
 * function to do a sleep() between each byte or two. Remember that
 * tar pitting will tie up own resources as well, so the number of
 * tar pitted connections should probably be limited.
 * - A tar pitted connection can also send rubbish back to the cracker.
 * That is good for some reasons, bad for other. The worst part is
 * if we've misinterpreted the clients status, ie it is a regular client
 * with e.g. a very slow connection.
 *
 * Important:
 * - We must be able to remove a black listed client from the black list.
 * - We should probably never black list a client which already has
 * explicit connect permission. See tcp_server's client_can_connect().
 * - We must report that we black list a client (syslog()?)
 *
 * 2003-07-15, boa
 */

/*
 * I'm now in the process of transfering some properties from request_t
 * to connection, and thinking that maybe some other props should
 * be associated with the connection, e.g. protocol (HTTP 1.0, 1.1, SHTTP 1.3)
 * Then is is easier to switch protocols.
 */

/*
 * Implementation of the connection ADT.
 */

struct connection_tag {
	int timeout_reads, timeout_writes;
	int retries_reads, retries_writes;
	int persistent;
	meta_socket sock;
	void *arg2;

	/* Client we're connected with */
	struct sockaddr_in addr;

	membuf readbuf;
	membuf writebuf;

	/*
	 * We need to count incoming and outgoing bytes. Outgoing bytes
	 * are important for HTTP logging. Incoming bytes are used to
	 * detect DoS attacks. We therefore need to compute
	 * the byte-per-second ratio to be able to disconnect very
	 * slow clients. We don't need subsecond precision so we
	 * can use time_t. We use two time_t's, one to be able to
	 * disconnect clients that has been connected too long, another to
	 * track progress for the current request.
	 * 2002-06-15 boa
	 */
	size_t incoming_bytes, outgoing_bytes;
	time_t conn_established, request_started;

};

/* Local helpers */
static inline int fill_read_buffer(connection conn)
{
	int success;
	size_t nread;

	assert(conn != NULL);

	/* Clear the read buffer */
	membuf_reset(conn->readbuf);

	success = sock_read(
		conn->sock,
		membuf_data(conn->readbuf),
		membuf_size(conn->readbuf),
		conn->timeout_reads,
		conn->retries_reads,
		&nread);

	/* NOTE: errors may indicate bad clients */
	if (success) {
		if (nread == 0) {
			errno = EAGAIN;
			success = 0;
		}
		else {
			conn->incoming_bytes += nread;
			membuf_set_written(conn->readbuf, nread);
		}
	}

	return success;
}

static inline void reset_counters(connection conn)
{
	assert(conn != NULL);
	conn->incoming_bytes = conn->outgoing_bytes = 0;
	conn->conn_established = conn->request_started = time(NULL);
}

static inline void
add_to_writebuf(connection conn, const void *buf, size_t count)
{
	size_t nwritten;

	/* There must be space in the write buffer */
	assert(membuf_canwrite(conn->writebuf) >= count);

	nwritten = membuf_write(conn->writebuf, buf, count);
	assert(count == nwritten);

}

static inline size_t
copy_from_readbuf(connection conn, void *buf, size_t count)
{
	return membuf_read(conn->readbuf, buf, count);
}

static inline int
readbuf_contains_atleast(connection conn, size_t count)
{
	size_t n = membuf_canread(conn->readbuf);
	return (n >= count) ? 1 : 0;
}

static inline int
readbuf_contains_data(connection conn)
{
	size_t count = membuf_canread(conn->readbuf);
	return count != 0 ? 1 : 0;
}

static inline int
readbuf_empty(connection conn)
{
	return readbuf_contains_data(conn) == 0;
}

static inline int
writebuf_has_room_for(connection conn, size_t count)
{
	size_t n = membuf_canwrite(conn->writebuf);
	return n >= count ? 1 : 0;
}

static inline int conn_getc(connection conn)
{
	char c;

	assert(NULL != conn);
	assert(membuf_canread(conn->readbuf) > 0);

	if (membuf_read(conn->readbuf, &c, 1) == 1)
		return (int)c;
	else
		return EOF;
}

connection connection_new(
	int timeout_reads,
	int timeout_writes,
	int retries_reads,
	int retries_writes,
	void *arg2)
{
	connection p;

	assert(timeout_reads >= 0);
	assert(timeout_writes >= 0);
	assert(retries_reads >= 0);
	assert(retries_writes >= 0);

	/* Allocate memory needed */
	if ((p = calloc(1, sizeof *p)) != NULL) {
		p->readbuf = NULL;
		p->writebuf = NULL;
		p->persistent = 0;
		p->timeout_reads = timeout_reads;
		p->timeout_writes = timeout_writes;
		p->retries_reads = retries_reads;
		p->retries_writes = retries_writes;
		p->arg2 = arg2;
		p->sock = NULL;
		reset_counters(p);
	}

	return p;
}

int connection_connect(connection c, const char *host, int port)
{
	assert(c != NULL);

	if ((c->sock = create_client_socket(host, port)) == NULL)
		return 0;
	else
		return 1;
}

membuf connection_reclaim_read_buffer(connection conn)
{
	membuf mb;

	assert(conn != NULL);
	assert(conn->readbuf != NULL); /* Don't reclaim twice */

	mb = conn->readbuf;
	conn->readbuf = NULL;
	return mb;
}

membuf connection_reclaim_write_buffer(connection conn)
{
	membuf mb;
	assert(conn != NULL);
	assert(conn->writebuf != NULL);

	mb = conn->writebuf;
	conn->writebuf = NULL;
	return mb;
}

void connection_assign_read_buffer(connection conn, membuf buf)
{
	assert(conn != NULL);
	assert(buf != NULL);
	assert(conn->readbuf == NULL); /* Don't assign without reclaiming the old one first */

	conn->readbuf = buf;
}

void connection_assign_write_buffer(connection conn, membuf buf)
{
	assert(conn != NULL);
	assert(buf != NULL);
	assert(conn->writebuf == NULL); /* Don't assign without reclaiming the old one first */

	conn->writebuf = buf;
}


int connection_flush(connection conn)
{
	int success = 1;
	size_t count;

	assert(conn != NULL);

	count = membuf_canread(conn->writebuf);
	if (count > 0) {
		success = sock_write(
			conn->sock,
			membuf_data(conn->writebuf),
			count,
			conn->timeout_writes,
			conn->retries_writes);

		if (success) {
			conn->outgoing_bytes += count;
			membuf_reset(conn->writebuf);
		}
	}

	return success;
}

int connection_close(connection conn)
{
	int rc, flush_success, close_success;

	assert(conn != NULL);

	flush_success = connection_flush(conn);
	close_success = sock_close(conn->sock);

	if (!flush_success)
		rc = 0;
	else if (!close_success)
		rc = 0;
	else
		rc = 1;

	return rc;
}

int connection_getc(connection conn, int *pchar)
{
	int success = 1;

	assert(conn != NULL);
	assert(pchar != NULL);

	/* Fill buffer if empty */
	if (readbuf_empty(conn))
		success = fill_read_buffer(conn);

	/* Get one character from buffer */
	if (success) {
		*pchar = conn_getc(conn);
		if (*pchar == EOF)
			success = 0;
	}

	return success;
}

static inline int
write_to_socket(connection conn, const char *buf, size_t count)
{
	return sock_write(
		conn->sock,
		buf,
		count,
		conn->timeout_writes,
		conn->retries_writes);
}

/*
 * Write count bytes to the buffer.
 * First of all we flush the buffer if there isn't room for the
 * incoming data. If the buffer still has no room for the incoming
 * data, we write the data directly to the socket.
 * We return 0 for errors and 1 for success.
 */
int connection_write(connection conn, const void *buf, size_t count)
{
	int success = 1;

	assert(conn != NULL);
	assert(buf != NULL);

	if (!writebuf_has_room_for(conn, count))
		success = connection_flush(conn);

	if (!success)
		;
	else if (writebuf_has_room_for(conn, count))
		add_to_writebuf(conn, buf, count);
	else if ((success = write_to_socket(conn, buf, count)))
		conn->outgoing_bytes += count;

	return success;
}


/* This function is used whenever we want to bypass the read buffer.
 * That's relevant e.g. when we read more than the size of the buffer
 * in one chunk. Doesn't happen very often, but it may happen.
 *
 * Anyway, this is the reason we do not update the conn->iReadUntil
 * in this function.
 */

/*
 * Here is where we have to measure bps for incoming
 * data. All we have to do is to do a time(NULL) or clock() before
 * and after the call to sock_read(). Then we can compute
 * the duration and compare it with the number of bytes
 * read from the socket.
 *
 * The hard part is to set up general rules on how
 * to categorize our connected clients. Starting to
 * disconnect valid users will not be very popular.
 *
 * Remember that we cannot mark an IP as unwanted on the
 * connection level. Due to the pooling of connections the
 * villain may get another connection object the nex time.
 * The proper place is the parent object, the tcp_server.
 * We can either report an IP to the tcp_server or the tcp_server
 * can scan its connections for bad guys.
 */
static int read_from_socket(connection conn, void *pbuf, size_t count)
{
	int success;
	size_t nread;

	assert(conn != NULL);
	assert(pbuf != NULL);
	assert(readbuf_empty(conn));

	success = sock_read(
		conn->sock,
		pbuf,
		count,
		conn->timeout_reads,
		conn->retries_reads,
		&nread);

	if (success) {
		if (nread != count) {
			errno = EAGAIN;
			success = 0;
		}
		else {
			conn->incoming_bytes += nread;
		}
	}

	return success;
}

int connection_read(connection conn, void *buf, size_t count)
{
	int success = 1;
	size_t ncopied;
	char *cbuf;

	assert(conn != NULL);
	assert(buf != NULL);

	ncopied = copy_from_readbuf(conn, buf, count);

	/* We need a char buffer to be able to compute offsets */
	cbuf = buf;

	/* Were all bytes copied from cache? If so, return. */
	if (ncopied == count)
		return 1;

	count -= ncopied;
	cbuf += ncopied;

	/* Read from socket if buffer is too small anyway */
	if (membuf_size(conn->readbuf) < count) {
		success = read_from_socket(conn, cbuf, count);
	}
	else {
		/* Fill the read buffer by reading data from the socket.
		 * Then copy data from the read buffer to buf.
		 */
		success = fill_read_buffer(conn);
		if (success) {
			if (readbuf_contains_atleast(conn, count)) {
				if (copy_from_readbuf(conn, cbuf, count) != count) {
					errno = EAGAIN;
					success = 0;
				}
			}
			else {
				errno = EAGAIN;
				success = 0;
			}
		}
	}

	return success;
}

void *connection_arg2(connection conn)
{
	assert(conn != NULL);
	return conn->arg2;
}

void connection_discard(connection conn)
{
	assert(conn != NULL);

	/* Close the socket, ignoring any messages */
	sock_close(conn->sock);
	reset_counters(conn);
}

int connection_ungetc(connection conn, int c)
{
	int rc;

	(void)c;
	assert(conn != NULL);
	rc = membuf_unget(conn->readbuf);
	return rc == 1 ? 0 : -1;
}

void connection_set_persistent(connection conn, int val)
{
	assert(conn != NULL);
	conn->persistent = val;
}

int connection_is_persistent(connection conn)
{
	assert(conn != NULL);
	return conn->persistent;
}

struct sockaddr_in* connection_get_addr(connection conn)
{
	assert(conn != NULL);
	return (struct sockaddr_in*)&conn->addr;
}

void connection_free(connection conn)
{
	if (conn != NULL) {
		free(conn);
	}
}

void connection_set_params(connection conn, meta_socket sock, struct sockaddr_in* paddr)
{
	assert(conn != NULL);

	conn->sock = sock;
	conn->addr = *paddr;

	reset_counters(conn);
}

void connection_recycle(connection conn)
{
	assert(conn != NULL);

	conn->persistent = 0;
	conn->sock = NULL; // Maybe free it too?

	reset_counters(conn);
}

int data_on_socket(connection conn)
{
	assert(conn != NULL);

	return wait_for_data(conn->sock, conn->timeout_reads);
}

int connection_putc(connection conn, int ch)
{
	char c = (char)ch;
	assert(conn != NULL);

	return connection_write(conn, &c, 1);
}

int connection_puts(connection conn, const char *s)
{
	assert(conn != NULL);
	assert(s != NULL);
	return connection_write(conn, s, strlen(s));
}

int connection_gets(connection conn, char *dest, size_t destsize)
{
	int success = 1, c;
	size_t i = 0;

	assert(conn != NULL);
	assert(dest != NULL);
	assert(destsize > 0);

	while (i < destsize && (success = connection_getc(conn, &c))) {
		*dest++ = c;
		i++;

		if (c == '\n')
			break;
	}

	if (success && i < destsize)
		*dest = '\0';

	return success;
}

int connection_write_big_buffer(
	connection conn,
	const void *buf,
	size_t count,
	int timeout,
	int nretries)
{
	int success;

	assert(conn != NULL);
	assert(buf != NULL);
	assert(timeout >= 0);
	assert(nretries >= 0);

	if ((success = connection_flush(conn)))
		success = sock_write(conn->sock, buf, count, timeout, nretries);

	return success;
}
