// Simple page-serving httpd

#include <stdio.h>
#include <unistd.h>

#include <highlander.h>
#include <miscssl.h>
#include <meta_convert.h>

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
static int g_portno = 2000;

static void parse_command_line(int argc, char *argv[])
{
    const char *options = "hp:";

    int c;

    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'p':
                if (!isint(optarg) || !toint(optarg, &g_portno))
                    die("Port number must be an integer\n");
                break;

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

    // Some sanity checks
    if (g_portno == 0 || g_portno > 65535)
        die("Port number out of range(%d)\n", g_portno);
}

int main(int argc, char *argv[])
{
    http_server s;
    parse_command_line(argc, argv);

    s = http_server_new(m_servertype);
    http_server_set_port(s, g_portno);
    if (!http_server_alloc(s))
        die("Could not allocate resources\n");

    http_server_set_can_read_files(s, 1);

    if (!http_server_get_root_resources(s))
        die("Could not get root resources\n");

    if (!http_server_start(s))
        die("An error occured\n");

    return 0;
}
