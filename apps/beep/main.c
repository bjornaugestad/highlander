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
#include <db_user.h>

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
    return readbuf_header(conn, h);
}

// Read a User object off the stream. Format is { id name nick email },
// but id isn't really needed as we're inserting a new user to the db and will
// create the unique id using bdb sequence. 
static status_t user_add_handler(connection conn)
{
    char buf[1024];
    assert(sizeof buf >= user_size());
    User u = user_init(buf);


    if (!user_recv(u, conn)) {
        return failure;
    }

    // We get the db object via the connection's optional argument arg2.
    bdb_server db = connection_arg2(conn);
    assert(db != NULL);

    dbid_t id = bdb_user_add(db, u);

    // Send a reply to the client. KISS...
    struct beep_reply r = { BEEP_VERSION, BEEP_USER_ADD, 0};

    if (!writebuf_reply(conn, &r)) {
        fprintf(stderr, "Could not send reply\n");
        return failure;
    }

    // Gotta write the new dbid too.
    if (!writebuf_object_start(conn)
    ||  !writebuf_uint64(conn, id)
    ||  !writebuf_object_end(conn)
    ||  !connection_flush(conn)) {
        fprintf(stderr, "Could not send new id\n");
        return failure;
    }

    return success;
}

// This callback function kinda expects a serial format defined by us. We have our cbuf code
// already and can amend to that code. We need header parsing, requests, 
//
static void *beep_callback(void *arg)
{
    connection conn = arg;
    struct beep_header h;

    while (read_header(conn, &h)) {

        if (h.version != BEEP_VERSION) {
            fprintf(stderr, "Unknown protocol version %d\n", h.version);
            return NULL;
        }

        status_t status = failure;
        switch(h.request) {
            case BEEP_USER_ADD: 
                status = user_add_handler(conn);
                break;

            default:
                fprintf(stderr, "Unknown request %d\n", h.request);
                break;
        }

        if (status != success)
            break;
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    parse_command_line(argc, argv);
    bdb_server db = bdb_server_new();
    tcp_server srv = tcp_server_new(SOCKTYPE_TCP); // Chill with TLS
    tcp_server_set_worker_threads(srv, 4);
    tcp_server_set_queue_size(srv, 10);
    tcp_server_set_block_when_full(srv, true);
    tcp_server_set_retries(srv, 0, 0);


    // Set this one early as it's assigned when connection pool is contructed
    tcp_server_set_service_function(srv, beep_callback, db);
    process p = process_new("beep");

    tcp_server_set_port(srv, 3000);
    if (!tcp_server_init(srv))
        die("tcp_server_init");

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
