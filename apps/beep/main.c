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

// This callback function kinda expects a serial format defined by us. We have our cbuf code
// already and can amend to that code. We need header parsing, requests, 
static void *fn(void *arg)
{
    connection c = arg;
    (void)c;

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
