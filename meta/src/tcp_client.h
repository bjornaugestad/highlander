#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <connection.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Declaration of our tcp_client ADT */
typedef struct tcp_client_tag* tcp_client;

/* Creation and destruction */
tcp_client tcp_client_new(int socktype)
    __attribute__((warn_unused_result))
    __attribute__((malloc));

void tcp_client_free(tcp_client srv);

status_t tcp_client_set_hostname(tcp_client srv, const char *host)
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1)));

void tcp_client_set_port(tcp_client srv, uint16_t port)
    __attribute__((nonnull));

status_t tcp_client_connect(tcp_client this, const char *host, uint16_t port);

// We need these properties when we create SSL sockets, so
// set them before calling tcp_client_get_root_resources(),
// which is the function creating an SSL client socket.
status_t tcp_client_set_rootcert(tcp_client p, const char *path)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t tcp_client_set_private_key(tcp_client p, const char *path)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t tcp_client_set_ciphers(tcp_client p, const char *ciphers)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t tcp_client_set_ca_directory(tcp_client p, const char *path)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

connection tcp_client_connection(tcp_client p);
status_t tcp_client_close(tcp_client p);

unsigned tcp_client_get_retries_write(tcp_client this);
unsigned tcp_client_get_retries_read(tcp_client this);
unsigned tcp_client_get_timeout_write(tcp_client this);
unsigned tcp_client_get_timeout_read(tcp_client this);
void tcp_client_set_timeout_write(tcp_client this, unsigned millisec);
void tcp_client_set_timeout_read(tcp_client this, unsigned millisec);
void tcp_client_set_retries_read(tcp_client this, unsigned count);
void tcp_client_set_retries_write(tcp_client this, unsigned count);
void tcp_client_set_writebuf_size(tcp_client p, size_t size);
void tcp_client_set_readbuf_size(tcp_client p, size_t size);


#ifdef __cplusplus
}
#endif

#endif
