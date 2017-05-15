/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <stdbool.h>
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
#include <tcpsocket.h>

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
    tcpsocket sock;
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
static inline status_t fill_read_buffer(connection conn)
{
    ssize_t nread;

    assert(conn != NULL);
    assert(membuf_canread(conn->readbuf) == 0);

    /* Clear the read buffer */
    membuf_reset(conn->readbuf);

    nread = tcpsock_read(
        conn->sock,
        membuf_data(conn->readbuf),
        membuf_size(conn->readbuf),
        conn->timeout_reads,
        conn->retries_reads);

    /* NOTE: errors may indicate bad clients */
    if (nread == 0) {
        errno = EAGAIN;
    }
    else if (nread > 0) {
        conn->incoming_bytes += nread;
        membuf_set_written(conn->readbuf, nread);
    }

    return nread > 0 ? success : failure;
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

#ifdef NDEBUG
    (void)nwritten;
#endif
}

static inline size_t
copy_from_readbuf(connection conn, void *buf, size_t count)
{
    return membuf_read(conn->readbuf, buf, count);
}

static inline bool
readbuf_contains_atleast(connection conn, size_t count)
{
    size_t n = membuf_canread(conn->readbuf);
    return n >= count;
}

static inline bool
readbuf_contains_data(connection conn)
{
    size_t count = membuf_canread(conn->readbuf);
    return count != 0;
}

static inline bool
readbuf_empty(connection conn)
{
    return readbuf_contains_data(conn) == 0;
}

static inline bool
writebuf_has_room_for(connection conn, size_t count)
{
    size_t n = membuf_canwrite(conn->writebuf);
    return n >= count;
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
    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

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

    return p;
}

status_t connection_connect(connection c, const char *host, int port)
{
    assert(c != NULL);

    if ((c->sock = tcpsock_create_client_socket(host, port)) == NULL)
        return failure;

    return success;
}

membuf connection_reclaim_read_buffer(connection conn)
{
    membuf mb;

    assert(conn != NULL);

    mb = conn->readbuf;
    conn->readbuf = NULL;
    return mb;
}

membuf connection_reclaim_write_buffer(connection conn)
{
    membuf mb;
    assert(conn != NULL);

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


status_t connection_flush(connection conn)
{
    status_t status = success;
    size_t count;

    assert(conn != NULL);

    count = membuf_canread(conn->writebuf);
    if (count > 0) {
        status = tcpsock_write(
            conn->sock,
            membuf_data(conn->writebuf),
            count,
            conn->timeout_writes,
            conn->retries_writes);

        if (status) {
            conn->outgoing_bytes += count;
            membuf_reset(conn->writebuf);
        }
    }

    return status;
}

status_t connection_close(connection conn)
{
    status_t flush_success, close_success;

    assert(conn != NULL);

    flush_success = connection_flush(conn);
    close_success = tcpsock_close(conn->sock);

    if (!flush_success)
        return failure;

    if (!close_success)
        return failure;

    return success;
}

status_t connection_getc(connection conn, int *pc)
{
    char c;

    assert(conn != NULL);
    assert(pc != NULL);

    /* Fill buffer if empty */
    if (readbuf_empty(conn) && !fill_read_buffer(conn))
        return failure;

    /* Get one character from buffer */
    if (membuf_read(conn->readbuf, &c, 1) != 1)
        return failure;

    *pc = c;
    return success;
}

static inline status_t
write_to_socket(connection conn, const char *buf, size_t count)
{
    return tcpsock_write(
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
 */
status_t connection_write(connection conn, const void *buf, size_t count)
{
    assert(conn != NULL);
    assert(buf != NULL);

    if (!writebuf_has_room_for(conn, count) && !connection_flush(conn))
        return failure;

    if (writebuf_has_room_for(conn, count)) {
        add_to_writebuf(conn, buf, count);
        return success;
    }

    if (!write_to_socket(conn, buf, count))
        return failure;

    conn->outgoing_bytes += count;
    return success;
}


/*
 * Here is where we have to measure bps for incoming
 * data. All we have to do is to do a time(NULL) or clock() before
 * and after the call to tcpsock_read(). Then we can compute
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
static ssize_t read_from_socket(connection p, void *buf, size_t count)
{
    ssize_t nread;

    assert(p != NULL);
    assert(buf != NULL);
    assert(readbuf_empty(p));

    nread = tcpsock_read(p->sock, buf, count, p->timeout_reads, p->retries_reads);
    if (nread > 0)
        p->incoming_bytes += nread;

    return nread;
}

ssize_t connection_read(connection conn, void *buf, size_t count)
{
    size_t ncopied;
    ssize_t nread;

    /* We need a char buffer to be able to compute offsets */
    char *cbuf = buf;

    assert(conn != NULL);
    assert(buf != NULL);

    /* First copy data from the read buffer.
     * Were all bytes copied from buf? If so, return. */
    ncopied = copy_from_readbuf(conn, buf, count);
    if (ncopied == count)
        return ncopied;

    count -= ncopied;
    cbuf += ncopied;

    /* If the buffer can't hold the number of bytes we're
     * trying to read, there's no point in filling it. Therefore
     * we read directly from the socket if buffer is too small. */
    if (membuf_size(conn->readbuf) < count) {
        if ((nread = read_from_socket(conn, cbuf, count)) == -1)
            return -1;

        /* Return read count */
        return nread + ncopied;
    }

    /* If we end up here, we must first fill the read buffer
     * and then read from it. */
    if (!fill_read_buffer(conn))
        return -1;

    /* Now read as much as possible from the buffer, and 
     * return count, or possible short count, to our caller. */
    nread = copy_from_readbuf(conn, cbuf, count);
    return ncopied + nread;
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
    tcpsock_close(conn->sock);
    reset_counters(conn);
}

status_t connection_ungetc(connection conn, int c)
{
    (void)c;
    assert(conn != NULL);
    return membuf_unget(conn->readbuf);
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
    return &conn->addr;
}

void connection_free(connection conn)
{
    if (conn != NULL) {
        free(conn);
    }
}

void connection_set_params(connection conn, tcpsocket sock, struct sockaddr_in* paddr)
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

    return tcpsock_wait_for_data(conn->sock, conn->timeout_reads) == success;
}

status_t connection_putc(connection conn, int ch)
{
    char c = (char)ch;
    assert(conn != NULL);

    return connection_write(conn, &c, 1);
}

status_t connection_puts(connection conn, const char *s)
{
    assert(conn != NULL);
    assert(s != NULL);
    return connection_write(conn, s, strlen(s));
}

status_t connection_gets(connection conn, char *dest, size_t destsize)
{
    status_t status = success;
    int c;
    size_t i = 0;

    assert(conn != NULL);
    assert(dest != NULL);
    assert(destsize > 0);

    while (i < destsize && (status = connection_getc(conn, &c))) {
        *dest++ = c;
        i++;

        if (c == '\n')
            break;
    }

    if (status && i < destsize)
        *dest = '\0';

    return status;
}

status_t connection_write_big_buffer(
    connection conn,
    const void *buf,
    size_t count,
    int timeout,
    int nretries)
{
    status_t rc;

    assert(conn != NULL);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    if ((rc = connection_flush(conn))) {
        rc = tcpsock_write(conn->sock, buf, count, timeout, nretries);
    }

    return rc;
}
