
// This is where imapsd starts executing. What do we
// need to do? The usual stuff parsing command line
// arguments. Also, loading certs and key, connecting to
// db, daemonize ourselves. Quite a lot, actually.
//
// We start off using echoserver as a template.
#include <stdio.h>
#include <unistd.h>

#include <tcp_server.h>
#include <connection.h>
#include <meta_process.h>
#include <miscssl.h>

static void* imaps_handler(void* arg)
{
    connection c = arg;
    char buf[1024];

    while (connection_gets(c, buf, sizeof buf)) {
        if (!connection_puts(c, buf) || !connection_flush(c))
            warning("imaps:Could not echo input.\n");
    }

    return NULL;
}

static void* smtps_handler(void* arg)
{
    connection c = arg;
    char buf[1024];

    while (connection_gets(c, buf, sizeof buf)) {
        if (!connection_puts(c, buf) || !connection_flush(c))
            warning("smtps:Could not echo input.\n");
    }

    return NULL;
}

static void parse_command_line(int argc, char *argv[])
{
    const char *options = "h";

    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case '?':
            default:
                fprintf(stderr, "USAGE: %s [-h]\n", argv[0]);
                exit(1);
        }
    }
}

int main(int argc, char *argv[])
{
    process p;

    // We need two servers: imaps and smtps
    tcp_server imaps, smtps;

    if (!openssl_init())
        exit(1);
    parse_command_line(argc, argv);

    p = process_new("imapsd");
    imaps = tcp_server_new(SOCKTYPE_SSL);

    // smtps uses STARTTLS
    smtps = tcp_server_new(SOCKTYPE_TCP);

    if (!tcp_server_init(imaps) || !tcp_server_init(smtps))
        exit(2);

    tcp_server_set_rootcert(imaps, "./rootcert.pem");
    tcp_server_set_private_key(imaps, "./server.pem");
    tcp_server_set_service_function(imaps, imaps_handler, NULL);
    tcp_server_set_port(imaps, 993);
    tcp_server_start_via_process(p, imaps);

    tcp_server_set_rootcert(smtps, "./rootcert.pem");
    tcp_server_set_private_key(smtps, "./server.pem");
    tcp_server_set_service_function(smtps, smtps_handler, NULL);
    tcp_server_set_port(smtps, 25);
    tcp_server_start_via_process(p, smtps);

    if (!process_start(p, 0))
        exit(3);

    if (!process_wait_for_shutdown(p)) {
        perror("process_wait_for_shutdown");
        exit(4);
    }

    tcp_server_free(imaps);
    process_free(p);
    return 0;
}

