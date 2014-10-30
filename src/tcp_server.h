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

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

/* for size_t */
#include <stddef.h>

#include <meta_process.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Declaration of our tcp_server ADT */
typedef struct tcp_server_tag* tcp_server;

/* Creation and destruction */
tcp_server tcp_server_new(void);
int tcp_server_init(tcp_server srv);
void tcp_server_free(tcp_server srv);

/* Basic access control */
int  tcp_server_allow_clients(tcp_server srv, const char* filter);
void tcp_server_clear_client_filter(tcp_server srv);

/* resource management */
int tcp_server_get_root_resources(tcp_server srv);
int tcp_server_free_root_resources(tcp_server s);

/* Configuration */
int  tcp_server_set_hostname(tcp_server srv, const char* host);
void tcp_server_set_unix_socket(tcp_server s);
void tcp_server_set_port(tcp_server srv, int port);
void tcp_server_set_queue_size(tcp_server srv, size_t size);
void tcp_server_set_readbuf_size(tcp_server s, size_t size);
void tcp_server_set_writebuf_size(tcp_server s, size_t size);
void tcp_server_set_block_when_full(tcp_server srv, int block);
void tcp_server_set_worker_threads(tcp_server srv, size_t count);
void tcp_server_set_service_function(tcp_server srv, void* (*func)(void*), void* arg);
void tcp_server_set_timeout(tcp_server srv, int reads, int writes, int accepts);
void tcp_server_set_retries(tcp_server srv, int reads, int writes);

/* startup and shutdown */
int tcp_server_start(tcp_server srv);
int tcp_server_start_via_process(process p, tcp_server s);
int tcp_server_shutting_down(tcp_server srv);
int tcp_server_shutdown(tcp_server srv);

/* performance counters */
/* These three functions are wrapper functions for the threadpool
 * performance counters. */
unsigned long tcp_server_sum_blocked(tcp_server s);
unsigned long tcp_server_sum_discarded(tcp_server s);
unsigned long tcp_server_sum_added(tcp_server s);

/* Number of times poll() returned with errno == EINTR or EAGAIN */
unsigned long tcp_server_sum_poll_intr(tcp_server p);
unsigned long tcp_server_sum_poll_again(tcp_server p);

/* Number of times accept() returned -1 with errno equal to one of the MANY
 * error codes we ignore. The set of codes is platform specific and hence not
 * listed here.
 */
unsigned long tcp_server_sum_accept_failed(tcp_server p);

/* Number of clients denied due to client IP address filtering */
unsigned long tcp_server_sum_denied_clients(tcp_server p);

#ifdef __cplusplus
}
#endif

#endif /* guard */

