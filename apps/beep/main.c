#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <meta_process.h>
#include <tcp_server.h>
#include <connection.h>

#include <bdb_server.h>
#include <cbuf.h>

// Default type is SSL. We may enable TCP instead.
static int m_servertype = SOCKTYPE_SSL;

static void parse_command_line(int argc, char *argv[])
{
    const char *options = "t";

    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 't':
                m_servertype = SOCKTYPE_TCP;
                break;

            case '?':
            default:
                fprintf(stderr, "USAGE: %s [-t] where -t disables ssl(enables TCP)\n", argv[0]);
                exit(1);
        }
    }
}

static status_t read_header(connection c, struct header *h)
{
    if (!connection_read_u16(c, &h->version))
        return failure;

    if (!connection_read_u16(c, &h->request))
        return failure;

    if (!connection_read_u64(c, &h->payload_len))
        return failure;

    // Now payload, which needs to be dynamically allocated and freed, as in a readbuf.
    // If all's well, we have the # of bytes in payload_len. Byte order is still an issue.
    char *buf = malloc(h->payload_len);
    if (buf == NULL)
        return failure;

    if (connection_read(c, buf, h->payload_len) != (ssize_t)h->payload_len)
        return failure;


    fprintf(stderr, "bro, I got a valid-ish request!\n");
    return success;
}

// This callback function kinda expects a serial format defined by us. We have our cbuf code
// already and can amend to that code. We need header parsing, requests, 
static void *fn(void *arg)
{
    connection c = arg;
    struct header h;

    while (true) {
        if (!read_header(c, &h))
            break;
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    parse_command_line(argc, argv);
    bdb_server db = bdb_server_new();
    tcp_server srv = tcp_server_new(SOCKTYPE_TCP); // Chill with TLS
    process p = process_new("beep");

    tcp_server_set_port(srv, 3000);
    if (!tcp_server_init(srv))
        die("tcp_server_init");

    tcp_server_set_service_function(srv, fn, NULL);
    tcp_server_start_via_process(p, srv);

    status_t rc = process_add_object_to_start(p, db, 
        bdb_server_do_func,
        bdb_server_undo_func,
        bdb_server_run_func,
        bdb_server_shutdown_func);

    if (rc != success)
        die("process_add_object_to_start");

    if (!process_start(p, 0))
        die("process_start()");

    if (!process_wait_for_shutdown(p))
        die("process_wait_for_shutdown");

    bdb_server_free(db);
    process_free(p);
    return 0;
}
