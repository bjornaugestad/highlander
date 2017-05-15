#include <stdio.h>
#include <unistd.h>

#include <tcp_server.h>
#include <connection.h>
#include <meta_process.h>

// Default type is TCP. We may enable SSL instead.
static int m_servertype = SOCKTYPE_TCP;

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
    const char *options = "s";

    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 's':
                m_servertype = SOCKTYPE_SSL;
                break;

            case '?':
            default:
                fprintf(stderr, "USAGE: %s [-s] where -s enables ssl\n", argv[0]);
                exit(1);
        }
    }
}

int main(int argc, char *argv[])
{
    process p;
    tcp_server srv;

    parse_command_line(argc, argv);
    
    p = process_new("echoserver");
    srv = tcp_server_new(m_servertype);

    tcp_server_init(srv);
    tcp_server_set_service_function(srv, fn, NULL);
    tcp_server_start_via_process(p, srv);

    if (!process_start(p, 0))
        exit(1);

    if (!process_wait_for_shutdown(p)) {
        perror("process_wait_for_shutdown");
        exit(EXIT_FAILURE);
    }

    tcp_server_free(srv);
    process_free(p);
    return 0;
}
