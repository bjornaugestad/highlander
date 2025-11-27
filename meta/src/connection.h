/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdbool.h>
#include <stdint.h>

#include <meta_membuf.h>
#include <gensocket.h>
#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct connection_tag* connection;

connection connection_new(int socktype, unsigned timeout_reads, unsigned timeout_writes,
    unsigned retries_reads, unsigned retries_writes, void *arg2)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void connection_free(connection conn);
static inline void connection_freev(void *p) { connection_free(p); }

socket_t connection_socket(connection conn)
    __attribute__((nonnull, warn_unused_result));

void connection_recycle(connection conn)
    __attribute__((nonnull));

void *connection_arg2(connection conn)
    __attribute__((nonnull, warn_unused_result));

/* connect to a host on a port, return 0 on errors */
status_t connection_connect(connection c, const char *host, uint16_t port)
    __attribute__((nonnull, warn_unused_result));

status_t connection_printf(connection conn, const char *format, ...)
    __attribute__ ((format(printf, 2, 3)))
    __attribute__((nonnull, warn_unused_result));

status_t connection_putc(connection conn, int ch)
    __attribute__((nonnull, warn_unused_result));

status_t connection_puts(connection conn, const char *s)
    __attribute__((nonnull, warn_unused_result));

status_t connection_write(connection conn, const void *buf, size_t count)
    __attribute__((nonnull, warn_unused_result));

status_t connection_write_big_buffer(connection conn, const void *buf,
    size_t count, unsigned timeout, unsigned retries)
    __attribute__((nonnull, warn_unused_result));

status_t connection_flush(connection conn)
    __attribute__((nonnull, warn_unused_result));

ssize_t  connection_read(connection conn, void *buf, size_t bufsize)
    __attribute__((nonnull, warn_unused_result));

status_t connection_read_u16(connection conn, uint16_t* val)
    __attribute__((nonnull, warn_unused_result));

status_t connection_read_u32(connection conn, uint32_t* val)
    __attribute__((nonnull, warn_unused_result));

status_t connection_read_u64(connection conn, uint64_t* val)
    __attribute__((nonnull, warn_unused_result));

status_t connection_write_u16(connection conn, uint16_t val)
    __attribute__((nonnull, warn_unused_result));

status_t connection_write_u32(connection conn, uint32_t val)
    __attribute__((nonnull, warn_unused_result));

status_t connection_write_u64(connection conn, uint64_t val)
    __attribute__((nonnull, warn_unused_result));

status_t connection_getc(connection conn, char* pchar)
    __attribute__((nonnull, warn_unused_result));

status_t connection_gets(connection conn, char *buf, size_t bufsize)
    __attribute__((nonnull, warn_unused_result));

status_t connection_ungetc(connection conn, int c)
    __attribute__((nonnull, warn_unused_result));

status_t connection_close(connection conn)
    __attribute__((nonnull, warn_unused_result));

bool connection_is_persistent(connection conn)
    __attribute__((nonnull, warn_unused_result));

void connection_set_persistent(connection conn, bool val)
    __attribute__((nonnull));

void connection_set_params(connection conn, struct sockaddr_storage* paddr)
    __attribute__((nonnull));

struct sockaddr_storage* connection_get_addr(connection conn)
   __attribute__((nonnull, warn_unused_result));

int data_on_socket(connection conn)
    __attribute__((nonnull, warn_unused_result));

size_t connection_readbuf_size(connection conn)
    __attribute__((nonnull, warn_unused_result));

size_t connection_writebuf_size(connection conn)
    __attribute__((nonnull, warn_unused_result));

membuf connection_reclaim_read_buffer(connection conn)
    __attribute__((nonnull, warn_unused_result));

membuf connection_reclaim_write_buffer(connection conn)
    __attribute__((nonnull, warn_unused_result));

void connection_assign_read_buffer(connection conn, membuf buf)
    __attribute__((nonnull));

void connection_assign_write_buffer(connection conn, membuf buf)
    __attribute__((nonnull));

int connection_get_fd(connection conn)
    __attribute__((nonnull, warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
