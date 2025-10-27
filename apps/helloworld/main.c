#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <highlander.h>
#include <miscssl.h>
#include <http_response.h>
#include <http_server.h>

static int page_handler(http_request req, http_response page)
{
    const char *html =
        "<html><head><title>Hello, world</title></head>"
        "<body>Hello, world</body></html>";

    (void)req;

    if (!response_add(page, html))
        return HTTP_500_INTERNAL_SERVER_ERROR;

   return 0;
}

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
int main(int argc, char *argv[])
{
    parse_command_line(argc, argv);

    process p = process_new("helloworld");
    http_server s = http_server_new(m_servertype);
    http_server_set_port(s, 2000);

    if (!http_server_start_via_process(p, s))
       die("Internal error\n");

    if (!http_server_alloc(s))
        die("Could not allocate resources\n");

    if (!http_server_add_page(s, "/", page_handler, NULL))
        die("Could not add page.\n");

    if (m_servertype == SOCKTYPE_SSL) {
        if (!openssl_init())
            exit(1);

        if (!http_server_set_server_cert_chain_file(s, "./pki/server/server_chain.pem"))
            die("Meh. Could not set server cert\n");

        if (!http_server_set_private_key(s, "./pki/server/server.key"))
            die("Meh. Could not set private key\n");
    }

    if (!process_start(p, 0))
        die("An error occured\n");

    if (!process_wait_for_shutdown(p)) {
        perror("process_wait_for_shutdown");
        exit(4);
    }

    http_server_free(s);
    process_free(p);


    if (m_servertype == SOCKTYPE_SSL)
        return !!openssl_exit();

    return 0;
}
