/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef CONNECTION_H
#define CONNECTION_H

struct sockaddr_in;

#include <meta_membuf.h>
#include <meta_socket.h>
#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Declaration of our connection ADT */
typedef struct connection_tag* connection;

connection connection_new(int timeout_reads, int timeout_writes,
    int retries_reads, int retries_writes, void *arg2);

void connection_free(connection conn);

void connection_recycle(connection conn)
    __attribute__((nonnull(1)));

void *connection_arg2(connection conn)
    __attribute__((nonnull(1)));

/* connect to a host on a port, return 0 on errors */
status_t connection_connect(connection c, const char *host, int port)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t connection_putc(connection conn, int ch)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

status_t connection_puts(connection conn, const char *s)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t connection_write(connection conn, const void *buf, size_t count)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t connection_write_big_buffer(connection conn, const void *buf,
    size_t count, int timeout, int retries)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t connection_flush(connection conn)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

ssize_t connection_read(connection conn, void *buf, size_t bufsize)
    __attribute__((nonnull(1, 2)));

status_t connection_getc(connection conn, int* pchar)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t connection_gets(connection conn, char *buf, size_t bufsize)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

status_t connection_ungetc(connection conn, int c)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

void connection_discard(connection conn)
    __attribute__((nonnull(1)));

status_t connection_close(connection conn)
    __attribute__((nonnull(1)))
    __attribute__((warn_unused_result));

void connection_set_persistent(connection conn, int val)
    __attribute__((nonnull(1)));

int	connection_is_persistent(connection conn)
    __attribute__((nonnull(1)));

void connection_set_params(connection conn, tcpsocket sock,
    struct sockaddr_in* paddr)
    __attribute__((nonnull(1, 2, 3)));

struct sockaddr_in* connection_get_addr(connection conn)
    __attribute__((nonnull(1)));

int data_on_socket(connection conn)
    __attribute__((nonnull(1)));

size_t connection_readbuf_size(connection conn)
    __attribute__((nonnull(1)));

size_t connection_writebuf_size(connection conn)
    __attribute__((nonnull(1)));

membuf connection_reclaim_read_buffer(connection conn)
    __attribute__((nonnull(1)));

membuf connection_reclaim_write_buffer(connection conn)
    __attribute__((nonnull(1)));

void  connection_assign_read_buffer(connection conn, membuf buf)
    __attribute__((nonnull(1, 2)));

void  connection_assign_write_buffer(connection conn, membuf buf)
    __attribute__((nonnull(1, 2)));

int connection_get_fd(connection conn)
    __attribute__((nonnull(1)));


#ifdef __cplusplus
}
#endif

#endif
