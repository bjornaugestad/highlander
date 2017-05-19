#include <stdio.h>
#include <unistd.h>

#include <tcp_server.h>
#include <connection.h>
#include <meta_process.h>
#include <miscssl.h>

// Default type is SSL. We may enable TCP instead.
static int m_servertype = SOCKTYPE_SSL;

static void* fn(void* arg)
{
    connection c = arg;
    char buf[1024];

    while (connection_gets(c, buf, sizeof buf)) {
        if (!connection_puts(c, buf) || !connection_flush(c))
            warning("Could not echo input.\n");
    }

    return NULL;
}

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

int main(int argc, char *argv[])
{
    process p;
    tcp_server srv;

    if (!openssl_init())
        exit(1);
    parse_command_line(argc, argv);

    p = process_new("echoserver");
    srv = tcp_server_new(m_servertype);

    if (!tcp_server_init(srv))
        exit(2);

    tcp_server_set_rootcert(srv, "./rootcert.pem");
    tcp_server_set_private_key(srv, "./server.pem");
    tcp_server_set_service_function(srv, fn, NULL);
    tcp_server_start_via_process(p, srv);

    if (!process_start(p, 0))
        exit(3);

    if (!process_wait_for_shutdown(p)) {
        perror("process_wait_for_shutdown");
        exit(4);
    }

    tcp_server_free(srv);
    process_free(p);
    return 0;
}
