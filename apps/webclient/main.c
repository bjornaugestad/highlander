/*
 * For now just a simple HTTP client program used to
 * try out the new parsing of responses in highlander.
 */

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <meta_common.h>
#include <meta_convert.h>
#include <highlander.h>
#include <connection.h>

/* A couple of globals */

/* How many client threads will be created? */
size_t g_nthreads = 1;
int g_nrequests = 1;
int g_print_contents = 0;
int g_print_header = 0;

const char*g_hostname = NULL;
uint16_t g_port = 0;
const char* g_uri = "/";
  
// timeout is in ms.
unsigned timeout_reads = 200, timeout_writes = 50;
unsigned nretries_read = 1, nretries_write = 1;


static void die_error(const char *context, error e)
{
    fprintf(stderr, "%s:", context);
    if (has_error_message(e))
        fprintf(stderr, "%s\n", get_error_message(e));

    fprintf(stderr, "strerror returns %s for %d, but that may be misleading.\n",
        strerror(get_error_code(e)), get_error_code(e));

    exit(EXIT_FAILURE);
}

static void show_help(void)
{
    printf("USAGE: webclient [-t n -r n -u uri -HCv] host port\n");
    printf("   -t n thread count. Default is 1 threads.\n");
    printf("   -r n request count. Default is 1 requests per thread.\n");
    printf("   -H print response header\n");
    printf("   -C print response content\n");
    printf("   -T ms Timeout in millisecs\n");
    printf("   -R n  Number of retries per read/write op\n");
    printf("   -v Be verbose\n");
}

static void parse_commandline(int argc, char *argv[])
{
    int c;

    while ( (c = getopt(argc, argv, "vu:t:r:hCHT:R:")) != EOF) {
        switch (c) {
            case 'v':
                meta_verbose_level++;
                break;

            case 'u':
                g_uri = optarg;
                break;

            case 'T':
                if (!isuint(optarg) || !touint(optarg, &timeout_writes))
                    die("Timeout must be an integer\n");
                timeout_reads = timeout_writes;
                break;

            case 'R':
                if (!isuint(optarg) || !touint(optarg, &nretries_write))
                    die("Retries must be an integer\n");
                nretries_read = nretries_write;
                break;

            case 't':
                if (!issize_t(optarg) || !tosize_t(optarg, &g_nthreads))
                    die("number of threads must be a positive integer\n");

                break;

            case 'r':
                if (!isint(optarg) || !toint(optarg, &g_nrequests))
                    die("Number of requests must be an integer\n");

                break;

            case 'C':
                g_print_contents = 1;
                break;

            case 'H':
                g_print_header = 1;
                break;

            case 'h':
                show_help();
                exit(EXIT_SUCCESS);

            case '?':
            default:
                show_help();
                exit(EXIT_FAILURE);
        }
    }

    /* Now get host and port */
    if (argc - optind != 2) {
        show_help();
        exit(EXIT_FAILURE);
    }

    g_hostname = argv[optind++];

    if (!isuint16_t(argv[optind]) || !touint16_t(argv[optind], &g_port))
        die("Port number must be a positive integer\n");
}

static void print_response_contents(http_response response)
{
    size_t cb = response_get_content_length(response);
    const char *s = response_get_entity(response);

    while (cb--)
        putchar(*s++);
}

static void* threadfunc(void* arg)
{
    (void)arg;

    http_request request = request_new();
    http_response response = response_new();
    connection conn = connection_new(SOCKTYPE_TCP, timeout_reads, timeout_writes,
        nretries_read, nretries_write, NULL);

    membuf rb = membuf_new(10000);
    membuf wb = membuf_new(10000);
    error e = error_new();

    connection_assign_read_buffer(conn, rb);
     connection_assign_write_buffer(conn, wb);

    verbose(1, "Connecting to host %s at port %d\n", g_hostname, g_port);
    if (!connection_connect(conn, g_hostname, g_port)) {
        fprintf(stderr, "Could not connect to %s:%d\n", g_hostname, g_port);
        pthread_exit(NULL);
    }

    /* Populate the request object and then send it to the server */
    request_set_method(request, METHOD_GET);
    request_set_version(request, VERSION_11);
    if (!request_set_host(request, g_hostname)
    || !request_set_uri(request, g_uri)
    || !request_set_user_agent(request, "My test program"))
        die("Could not set request properties.");

    for (int i = 0; i < g_nrequests; i++) {
        verbose(1, "Sending request for uri %s\n", g_uri);
        if (!request_send(request, conn, e))
            die_error("Could not send request to server", e);

        /* Now read the response back from the server */
        if (!response_receive(response, conn, 10 * 1024 * 1024, e))
            die_error("Could not receive response from server", e);

        verbose(1, "Got response from server.\n");

        if (g_print_header)
            response_dump(response, stdout);

        if (g_print_contents)
            print_response_contents(response);

        response_recycle(response);
    }

    request_free(request);
    response_free(response);

    if (!connection_close(conn))
        warning("Could not close connection\n");

    connection_free(conn);
    membuf_free(rb);
    membuf_free(wb);
    error_free(e);

    return NULL;
}

int main(int argc, char *argv[])
{
    parse_commandline(argc, argv);

    if (g_nthreads == 1)
        threadfunc(NULL);
    else {
        // allocate space for n thread id's, start threads and then join them

        size_t cb = sizeof *threads * g_nthreads;
        pthread_t *threads = malloc(cb);
        if ( (threads == NULL)
            die("Out of memory\n");

        for (size_t i = 0; i < g_nthreads; i++) {
            if (pthread_create(&threads[i], NULL, threadfunc, NULL))
                die("Could not start thread\n");
        }

        // Join the threads
        for (size_t i = 0; i < g_nthreads; i++) {
            if (pthread_join(threads[i], NULL))
                die("Could not join thread\n");
        }

        free(threads);
    }

    return 0;
}
