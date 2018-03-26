// Simple page-serving httpd

#include <stdio.h>
#include <unistd.h>

#include <highlander.h>
#include <miscssl.h>

static void show_usage(void)
{
    static const char *text[] = {
    "USAGE: httpd [-h]",
    ""
    };

    size_t i, n = sizeof text / sizeof *text;
    for (i = 0; i < n; i++)
        puts(text[i]);
}

static int m_servertype = SOCKTYPE_TCP;

static void parse_command_line(int argc, char *argv[])
{
    const char *options = "h";

    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'h':
                show_usage();
                exit(0);
                break;

            case '?':
            default:
                show_usage();
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

    http_server_set_can_read_files(s, 1);

    if (!http_server_get_root_resources(s))
        die("Could not get root resources\n");

    if (!http_server_start(s))
        die("An error occured\n");

    return 0;
}
