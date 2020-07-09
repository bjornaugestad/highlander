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

    int timeout_reads, timeout_writes, nretries_read, nretries_write;
};

tcp_client tcp_client_new(int socktype, void *context)
{
    tcp_client new;

    if ((new = calloc(1, sizeof *new)) == NULL)
        return NULL;

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
        new->nretries_read, new->nretries_write, context);
    if (new->conn == NULL)
        goto memerr;

    connection_assign_read_buffer(new->conn, new->readbuf);
    connection_assign_write_buffer(new->conn, new->writebuf);

    return new;

memerr:
    membuf_free(new->readbuf);
    membuf_free(new->writebuf);
    connection_free(new->conn);
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

