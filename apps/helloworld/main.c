#include <stdio.h>
#include <unistd.h>

#include <highlander.h>
#include <miscssl.h>

int page_handler(http_request req, http_response page)
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
    http_server s;
    parse_command_line(argc, argv);

    s = http_server_new(m_servertype);
    http_server_set_port(s, 2000);
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

    if (!http_server_get_root_resources(s))
        die("Could not get root resources\n");

    if (!http_server_start(s))
        die("An error occured\n");

    return 0;
}
