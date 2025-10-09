/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stddef.h>

#include <meta_process.h>
#include <gensocket.h>      // for SOCKTYPE_

#ifdef __cplusplus
extern "C" {
#endif

/* Declaration of our tcp_server ADT */
typedef struct tcp_server_tag* tcp_server;

/* Creation and destruction */
tcp_server tcp_server_new(int socktype)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

status_t tcp_server_init(tcp_server srv)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

void tcp_server_free(tcp_server srv);

/* Basic access control */
status_t tcp_server_allow_clients(tcp_server srv, const char *filter)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

void tcp_server_clear_client_filter(tcp_server srv)
    __attribute__((nonnull));

/* resource management */
status_t tcp_server_get_root_resources(tcp_server srv)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t tcp_server_free_root_resources(tcp_server srv)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

/* Configuration */
status_t tcp_server_set_hostname(tcp_server srv, const char *host)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

void tcp_server_set_unix_socket(tcp_server srv)
    __attribute__((nonnull));

void tcp_server_set_port(tcp_server srv, int port)
    __attribute__((nonnull));

void tcp_server_set_queue_size(tcp_server srv, size_t size)
    __attribute__((nonnull));

void tcp_server_set_readbuf_size(tcp_server srv, size_t size)
    __attribute__((nonnull));

void tcp_server_set_writebuf_size(tcp_server srv, size_t size)
    __attribute__((nonnull));

void tcp_server_set_block_when_full(tcp_server srv, int block)
    __attribute__((nonnull));

void tcp_server_set_worker_threads(tcp_server srv, size_t count)
    __attribute__((nonnull));

void tcp_server_set_service_function(tcp_server srv,
    void *(*func)(void*), void *arg)
    __attribute__((nonnull(1)));

void tcp_server_set_timeout(tcp_server srv, int reads, int writes, int accepts)
    __attribute__((nonnull));

void tcp_server_set_retries(tcp_server srv, int reads, int writes)
    __attribute__((nonnull));


/* startup and shutdown */
status_t tcp_server_start(tcp_server srv)
    __attribute__((nonnull));

status_t tcp_server_start_via_process(process p, tcp_server srv)
    __attribute__((nonnull));

int tcp_server_shutting_down(tcp_server srv)
    __attribute__((nonnull));

status_t tcp_server_shutdown(tcp_server srv)
    __attribute__((nonnull));

/* performance counters */
/* These three functions are wrapper functions for the threadpool
 * performance counters. */
unsigned long tcp_server_sum_blocked(tcp_server srv)
    __attribute__((nonnull));

unsigned long tcp_server_sum_discarded(tcp_server srv)
    __attribute__((nonnull));

unsigned long tcp_server_sum_added(tcp_server srv)
    __attribute__((nonnull));

/* Number of times poll() returned with errno == EINTR or EAGAIN */
unsigned long tcp_server_sum_poll_intr(tcp_server srv)
    __attribute__((nonnull));

unsigned long tcp_server_sum_poll_again(tcp_server srv)
    __attribute__((nonnull));


/* Number of times accept() returned -1 with errno equal to one of the MANY
 * error codes we ignore. The set of codes is platform specific and hence not
 * listed here.
 */
unsigned long tcp_server_sum_accept_failed(tcp_server srv)
    __attribute__((nonnull));


/* Number of clients denied due to client IP address filtering */
unsigned long tcp_server_sum_denied_clients(tcp_server srv)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));


// We need these properties when we create SSL sockets, so
// set them before calling tcp_server_get_root_resources(),
// which is the function creating an SSL server socket.
status_t tcp_server_set_private_key(tcp_server p, const char *path);
status_t tcp_server_set_cert_chain_file(tcp_server p, const char *path);
status_t tcp_server_set_ca_directory(tcp_server p, const char *path);

#ifdef __cplusplus
}
#endif

#endif
