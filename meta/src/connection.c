/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
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
#include <gensocket.h>
#include <cstring.h>

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

struct connection_tag {
    int timeout_reads, timeout_writes;
    int retries_reads, retries_writes;
    int persistent;
    socket_t sock;
    void *arg2;

    // tcp or ssl
    int socktype;

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
static inline status_t fill_read_buffer(connection this)
{
    ssize_t nread;

    assert(this != NULL);
    assert(membuf_canread(this->readbuf) == 0);

    /* Clear the read buffer */
    membuf_reset(this->readbuf);

    nread = socket_read(this->sock, membuf_data(this->readbuf),
        membuf_size(this->readbuf), this->timeout_reads,
        this->retries_reads);

    /* NOTE: errors may indicate bad clients */
    if (nread == 0) {
        errno = EAGAIN;
    }
    else if (nread > 0) {
        this->incoming_bytes += (size_t)nread;
        membuf_set_written(this->readbuf, (size_t)nread);
    }

    return nread > 0 ? success : failure;
}

static inline void reset_counters(connection this)
{
    assert(this != NULL);
    this->incoming_bytes = this->outgoing_bytes = 0;
    this->conn_established = this->request_started = time(NULL);
}

static inline void
add_to_writebuf(connection this, const void *buf, size_t count)
{
    size_t nwritten;

    /* There must be space in the write buffer */
    assert(membuf_canwrite(this->writebuf) >= count);

    nwritten = membuf_write(this->writebuf, buf, count);
    assert(count == nwritten);

#ifdef NDEBUG
    (void)nwritten;
#endif
}

static inline size_t
copy_from_readbuf(connection this, void *buf, size_t count)
{
    return membuf_read(this->readbuf, buf, count);
}

#if 0
clang says unused
static inline bool
readbuf_contains_atleast(connection this, size_t count)
{
    size_t n = membuf_canread(this->readbuf);
    return n >= count;
}
#endif

static inline bool
readbuf_contains_data(connection this)
{
    size_t count = membuf_canread(this->readbuf);
    return count != 0;
}

static inline bool
readbuf_empty(connection this)
{
    return readbuf_contains_data(this) == 0;
}

static inline bool
writebuf_has_room_for(connection this, size_t count)
{
    size_t n = membuf_canwrite(this->writebuf);
    return n >= count;
}

connection connection_new(
    int socktype,
    int timeout_reads,
    int timeout_writes,
    int retries_reads,
    int retries_writes,
    void *arg2)
{
    connection new;

    assert(socktype == SOCKTYPE_TCP || socktype == SOCKTYPE_SSL);
    assert(timeout_reads >= 0);
    assert(timeout_writes >= 0);
    assert(retries_reads >= 0);
    assert(retries_writes >= 0);

    if ((new = calloc(1, sizeof *new)) == NULL)
        return NULL;

    new->socktype = socktype;
    new->timeout_reads = timeout_reads;
    new->timeout_writes = timeout_writes;
    new->retries_reads = retries_reads;
    new->retries_writes = retries_writes;
    new->arg2 = arg2;
    reset_counters(new);

    return new;
}

status_t connection_connect(connection this, const char *host, int port)
{
    assert(this != NULL);

    if ((this->sock = socket_create_client_socket(this->socktype, this->arg2, host, port)) == NULL)
        return failure;

    return success;
}

membuf connection_reclaim_read_buffer(connection this)
{
    membuf mb;

    assert(this != NULL);

    mb = this->readbuf;
    this->readbuf = NULL;
    return mb;
}

membuf connection_reclaim_write_buffer(connection this)
{
    membuf mb;
    assert(this != NULL);

    mb = this->writebuf;
    this->writebuf = NULL;
    return mb;
}

void connection_assign_read_buffer(connection this, membuf buf)
{
    assert(this != NULL);
    assert(buf != NULL);
    assert(this->readbuf == NULL); /* Don't assign without reclaiming the old one first */

    this->readbuf = buf;
}

void connection_assign_write_buffer(connection this, membuf buf)
{
    assert(this != NULL);
    assert(buf != NULL);
    assert(this->writebuf == NULL); /* Don't assign without reclaiming the old one first */

    this->writebuf = buf;
}


status_t connection_flush(connection this)
{
    status_t rc = success;
    size_t count;

    assert(this != NULL);

    count = membuf_canread(this->writebuf);
    if (count > 0) {
        rc = socket_write(
            this->sock,
            membuf_data(this->writebuf),
            count,
            this->timeout_writes,
            this->retries_writes);

        if (rc) {
            this->outgoing_bytes += count;
            membuf_reset(this->writebuf);
        }
    }

    return rc;
}

status_t connection_close(connection this)
{
    status_t flush_success, close_success;

    assert(this != NULL);

    flush_success = connection_flush(this);
    close_success = socket_close(this->sock);

    if (!flush_success || !close_success)
        return failure;

    return success;
}

status_t connection_getc(connection this, int *pc)
{
    char c;

    assert(this != NULL);
    assert(pc != NULL);

    /* Fill buffer if empty */
    if (readbuf_empty(this) && !fill_read_buffer(this))
        return failure;

    /* Get one character from buffer */
    if (membuf_read(this->readbuf, &c, 1) != 1)
        return failure;

    *pc = c;
    return success;
}

static inline status_t
write_to_socket(connection this, const char *buf, size_t count)
{
    return socket_write(this->sock, buf, count,
        this->timeout_writes, this->retries_writes);
}

/*
 * Write count bytes to the buffer.
 * First of all we flush the buffer if there isn't room for the
 * incoming data. If the buffer still has no room for the incoming
 * data, we write the data directly to the socket.
 */
status_t connection_write(connection this, const void *buf, size_t count)
{
    assert(this != NULL);
    assert(buf != NULL);

    if (!writebuf_has_room_for(this, count) && !connection_flush(this))
        return failure;

    if (writebuf_has_room_for(this, count)) {
        add_to_writebuf(this, buf, count);
        return success;
    }

    if (!write_to_socket(this, buf, count))
        return failure;

    this->outgoing_bytes += count;
    return success;
}


/*
 * Here is where we have to measure bps for incoming
 * data. All we have to do is to do a time(NULL) or clock() before
 * and after the call to socket_read(). Then we can compute
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
static ssize_t read_from_socket(connection this, void *buf, size_t count)
{
    ssize_t nread;

    assert(this != NULL);
    assert(buf != NULL);
    assert(readbuf_empty(this));

    nread = socket_read(this->sock, buf, count, this->timeout_reads, this->retries_reads);
    if (nread > 0)
        this->incoming_bytes += (size_t)nread;

    return nread;
}

ssize_t connection_read(connection this, void *buf, size_t count)
{
    size_t ncopied;
    ssize_t nread;

    /* We need a char buffer to be able to compute offsets */
    char *cbuf = buf;

    assert(this != NULL);
    assert(buf != NULL);

    /* First copy data from the read buffer.
     * Were all bytes copied from buf? If so, return. */
    ncopied = copy_from_readbuf(this, buf, count);
    if (ncopied == count)
        return (ssize_t)ncopied;

    count -= ncopied;
    cbuf += ncopied;

    // If the buffer can't hold the number of bytes we're
    // trying to read, there's no point in filling it. Therefore
    // we read directly from the socket if buffer is too small.
    if (membuf_size(this->readbuf) < count) {
        if ((nread = read_from_socket(this, cbuf, count)) == -1)
            return -1;

        /* Return read count */
        return nread + (ssize_t)ncopied;
    }

    /* If we end up here, we must first fill the read buffer
     * and then read from it. */
    if (!fill_read_buffer(this))
        return -1;

    /* Now read as much as possible from the buffer, and
     * return count, or possible short count, to our caller. */
    nread = copy_from_readbuf(this, cbuf, count);
    return ncopied + nread;
}

void *connection_arg2(connection this)
{
    assert(this != NULL);
    return this->arg2;
}

void connection_discard(connection this)
{
    assert(this != NULL);

    /* Close the socket, ignoring any messages */
    socket_close(this->sock);
    this->sock = NULL;
    reset_counters(this);
}

status_t connection_ungetc(connection this, int c)
{
    (void)c;
    assert(this != NULL);
    return membuf_unget(this->readbuf);
}

void connection_set_persistent(connection this, int val)
{
    assert(this != NULL);
    this->persistent = val;
}

int connection_is_persistent(connection this)
{
    assert(this != NULL);
    return this->persistent;
}

struct sockaddr_in* connection_get_addr(connection this)
{
    assert(this != NULL);
    return &this->addr;
}

void connection_free(connection this)
{
    if (this != NULL) {
        free(this);
    }
}

void connection_set_params(connection this, socket_t _sock, struct sockaddr_in* paddr)
{
    assert(this != NULL);

    this->sock = _sock;
    this->addr = *paddr;

    reset_counters(this);
}

void connection_recycle(connection this)
{
    assert(this != NULL);

    // We need to close the socket here, if it still is open
    if (this->sock != NULL) {
        socket_close(this->sock);
        this->sock = NULL;
    }

    this->persistent = 0;
    reset_counters(this);
}

int data_on_socket(connection this)
{
    assert(this != NULL);

    return socket_wait_for_data(this->sock, this->timeout_reads) == success;
}

status_t connection_printf(connection conn, const char *fmt, ...)
{
    status_t status;
    va_list ap;
    cstring s = cstring_new();

    assert(conn != NULL);
    assert(fmt != NULL);

    va_start(ap, fmt);
    status = cstring_vprintf(s, fmt, ap);
    va_end(ap);

    if (status == success)
        status = connection_write(conn, c_str(s), cstring_length(s));
    cstring_free(s);
    return status;
}

status_t connection_putc(connection this, int ch)
{
    char c = (char)ch;
    assert(this != NULL);

    return connection_write(this, &c, 1);
}

status_t connection_puts(connection this, const char *s)
{
    assert(this != NULL);
    assert(s != NULL);
    if (connection_write(this, s, strlen(s)) && connection_flush(this))
        return success;
    else
        return failure;
}

status_t connection_gets(connection this, char *dest, size_t destsize)
{
    status_t rc = success;
    int c;
    size_t i = 0;

    assert(this != NULL);
    assert(dest != NULL);
    assert(destsize > 0);

    while (i < destsize && (rc = connection_getc(this, &c))) {
        *dest++ = c;
        i++;

        if (c == '\n')
            break;
    }

    if (rc && i < destsize)
        *dest = '\0';

    return rc;
}

status_t connection_write_big_buffer(
    connection this,
    const void *buf,
    size_t count,
    int timeout,
    int nretries)
{
    status_t rc;

    assert(this != NULL);
    assert(buf != NULL);
    assert(timeout >= 0);
    assert(nretries >= 0);

    if ((rc = connection_flush(this))) {
        rc = socket_write(this->sock, buf, count, timeout, nretries);
    }

    return rc;
}
