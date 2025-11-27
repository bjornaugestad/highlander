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
#include <beep_db.h>
#include <beep_user.h>

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

static status_t read_header(connection conn, struct beep_header *h)
{
    if (!connection_read_u16(conn, &h->version))
        return failure;

    if (!connection_read_u16(conn, &h->request))
        return failure;

    return success;
}

// Read a User object off the stream. Format is { id name nick email },
// but id isn't really needed as we're inserting a new user to the db and will
// create the unique id using bdb sequence. 
static void* user_add_handler(connection conn)
{
    User u = user_new();
    uint64_t id;
    char buf[1024]; // bigger than all fields in User

    if (!readbuf_object_start(conn))
        return NULL;

    if (!readbuf_uint64(conn, &id))
        return NULL;

    user_set_id(u, id);

    if (!readbuf_string(conn, buf, sizeof buf))
        return NULL;
    user_set_name(u, buf);

    if (!readbuf_string(conn, buf, sizeof buf))
        return NULL;
    user_set_nick(u, buf);

    if (!readbuf_string(conn, buf, sizeof buf))
        return NULL;
    user_set_email(u, buf);

    if (!readbuf_object_end(conn))
        return NULL;

    // Cool we now may have a deserialized User object.

    return NULL;
}

// This callback function kinda expects a serial format defined by us. We have our cbuf code
// already and can amend to that code. We need header parsing, requests, 
static void *beep_callback(void *arg)
{
    connection conn = arg;
    struct beep_header h;

    if (!read_header(conn, &h))
        return NULL;

    // We got a header
    printf("header.version %d request: %d\n", h.version, h.request);

    if (h.version != BEEP_VERSION) {
        fprintf(stderr, "Unknown protocol version %d\n", h.version);
        return NULL;
    }

    switch(h.request) {
        case BEEP_USER_ADD: return user_add_handler(conn);
        default:
            fprintf(stderr, "Unknown request %d\n", h.request);
            break;
    }

    // send something back
    if (!connection_write(conn, "hello", 5) || !connection_flush(conn)) {
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

    tcp_server_set_service_function(srv, beep_callback, NULL);
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
