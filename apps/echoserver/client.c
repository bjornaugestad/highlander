// ssl_echo_client.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

static void ssl_print_err(const char *where) {
    fprintf(stderr, "%s\n", where);
    ERR_print_errors_fp(stderr);
}

static int tcp_connect_any(const char *host, const char *port) {
    struct addrinfo hints, *res = NULL, *rp;
    int fd = -1, rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;     // try IPv6 then IPv4 (system order)
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_ADDRCONFIG;

    rc = getaddrinfo(host, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo(%s,%s): %s\n", host, port, gai_strerror(rc));
        return -1;
    }

    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return fd; // success
        }
        // else try next addr
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return -1;
}

static int ssl_write_all(SSL *ssl, const unsigned char *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        int n = SSL_write(ssl, buf + off, (int)(len - off));
        if (n <= 0) {
            int e = SSL_get_error(ssl, n);
            if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) continue;
            ssl_print_err("SSL_write");
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

int main(void) {
    const char *host = "localhost";
    const char *port = "3000";

    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IONBF, 0);
    SSL_load_error_strings();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { ssl_print_err("SSL_CTX_new"); return 1; }

    // Debug: disable cert verification
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    int fd = tcp_connect_any(host, port);
    if (fd < 0) {
        perror("connect");
        SSL_CTX_free(ctx);
        return 1;
    }

    SSL *ssl = SSL_new(ctx);
    if (!ssl) { ssl_print_err("SSL_new"); close(fd); SSL_CTX_free(ctx); return 1; }

    if (!SSL_set_tlsext_host_name(ssl, host)) {
        ssl_print_err("SSL_set_tlsext_host_name");
        SSL_free(ssl); close(fd); SSL_CTX_free(ctx); return 1;
    }
    if (SSL_set_fd(ssl, fd) != 1) {
        ssl_print_err("SSL_set_fd");
        SSL_free(ssl); close(fd); SSL_CTX_free(ctx); return 1;
    }
    if (SSL_connect(ssl) != 1) {
        ssl_print_err("SSL_connect");
        SSL_free(ssl); close(fd); SSL_CTX_free(ctx); return 1;
    }

    char line[8192];
    while (1) {
        if (!fgets(line, sizeof(line), stdin)) {
            if (ferror(stdin)) perror("fgets");
            int rc = SSL_shutdown(ssl); // send close_notify
            if (rc < 0) {
                int e = SSL_get_error(ssl, rc);
                if (e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE)
                    ssl_print_err("SSL_shutdown (first)");
            }
            while (rc == 0) { // wait peer close_notify
                rc = SSL_shutdown(ssl);
                if (rc < 0) {
                    int e = SSL_get_error(ssl, rc);
                    if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) continue;
                    ssl_print_err("SSL_shutdown (second)");
                    break;
                }
            }
            break;
        }

        size_t len = strlen(line);
        if (ssl_write_all(ssl, (unsigned char*)line, len) < 0) break;

        for (;;) {
            char buf[8192];
            int n = SSL_read(ssl, buf, (int)sizeof(buf));
            if (n > 0) {
                ssize_t w = write(STDOUT_FILENO, buf, (size_t)n);
                if (w < 0) { perror("write(stdout)"); break; }
                if (SSL_pending(ssl) > 0) continue;
                break;
            } else {
                int e = SSL_get_error(ssl, n);
                if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) continue;
                if (e == SSL_ERROR_ZERO_RETURN) goto out; // peer close_notify
                if (n == 0) goto out; // EOF
                ssl_print_err("SSL_read");
                goto out;
            }
        }
    }

out:
    shutdown(fd, SHUT_RDWR);
    SSL_free(ssl);
    close(fd);
    SSL_CTX_free(ctx);
    return 0;
}

