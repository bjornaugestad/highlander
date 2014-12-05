/*
 * For now just a simple HTTP client program used to
 * try out the new parsing of responses in highlander.
 */

#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <meta_misc.h>
#include <highlander.h>
#include <connection.h>

/* A couple of globals */

/* How many client threads will be created? */
int g_nthreads = 1;
int g_nrequests = 1;
int g_print_contents = 0;
int g_print_header = 0;
int g_verbose = 0;

const char*g_hostname = NULL;
int g_port = 0;
const char *g_uri = "/";

static void die_meta_error(const char* context, meta_error e)
{
	fprintf(stderr, "%s:", context);
	if(has_error_message(e))
		fprintf(stderr, "%s\n", get_error_message(e));

	fprintf(stderr, "strerror returns %s for %d, but that may be misleading.\n",
		strerror(get_error_code(e)), get_error_code(e));

	exit(EXIT_FAILURE);
}

static void verbose(int level, const char* fmt, ...)
{
	if(level > g_verbose)
		return;

	va_list ap;
	va_start(ap, fmt);

	vfprintf(stdout, fmt, ap);
	va_end(ap);
}

static void show_help(void)
{
	printf("USAGE: webclient [-t n -r n -u uri -HCv] host port\n");
	printf("   -t n thread count. Default is 1 threads.\n");
	printf("   -r n request count. Default is 1 requests per thread.\n");
	printf("   -H print response header\n");
	printf("   -C print response contents\n");
	printf("   -v Be verbose\n");
}

void parse_commandline(int argc, char *argv[])
{
	int c;

	while( (c = getopt(argc, argv, "vu:t:r:hCH")) != EOF) {
		switch(c) {
			case 'v':
				g_verbose++;
				break;

			case 'u':
				g_uri = optarg;
				break;

			case 't':
				g_nthreads = atoi(optarg);
				break;

			case 'r': 
				g_nrequests = atoi(optarg);
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
	if(argc - optind != 2) {
		show_help();
		exit(EXIT_FAILURE);
	}

	g_hostname = argv[optind++];
	g_port = atoi(argv[optind]);
}

static void print_response_contents(http_response response)
{
	size_t cb = response_get_content_length(response);
	const char* s = response_get_entity(response);
	
	while(cb--)
		putchar(*s++);
}

void* threadfunc(void* arg)
{
	// timeout is in ms. 
	int timeout_reads = 2000, timeout_writes = 50;
	int nretries_read = 4, nretries_write = 4;

	(void)arg;

	http_request request = request_new();
	http_response response = response_new();
	connection c = connection_new(timeout_reads, timeout_writes, 
		nretries_read, nretries_write, NULL);

	membuf rb = membuf_new(10000);
	membuf wb = membuf_new(10000);
	meta_error e = meta_error_new();

	connection_assign_read_buffer(c, rb);
	connection_assign_write_buffer(c, wb);

	verbose(1, "Connecting to host %s at port %d\n", g_hostname, g_port);
	if(!connection_connect(c, g_hostname, g_port)) {
		fprintf(stderr, "Could not connect to %s:%d\n", g_hostname, g_port);
		pthread_exit(NULL);
	}

	/* Populate the request object and then send it to the server */
	request_set_method(request, METHOD_GET);
	request_set_version(request, VERSION_11);
	request_set_host(request, g_hostname);
	request_set_uri(request, g_uri);
	request_set_user_agent(request, "My test program");

	for(int i = 0; i < g_nrequests; i++) {
		verbose(1, "Sending request for uri %s\n", g_uri);
		if(!request_send(request, c, e)) 
			die_meta_error("Could not send request to server", e);

		/* Now read the response back from the server */
		if(!response_receive(response, c, 1024 * 1024, e)) 
			die_meta_error("Could not receive response from server", e);

		verbose(1, "Got response from server.\n");

		if(g_print_header)
			response_dump(response, stdout);

		if(g_print_contents)
			print_response_contents(response);

		response_recycle(response);
	}

	request_free(request);
	response_free(response);

	connection_close(c);
	connection_free(c);
	membuf_free(rb);
	membuf_free(wb);
	meta_error_free(e);

	return NULL;
}

int main(int argc, char *argv[])
{
	parse_commandline(argc, argv);


	if(g_nthreads == 1)
		threadfunc(NULL);
	else {
		/* allocate space for n thread id's, start threads and then join them */
		pthread_t *threads;

		if( (threads = malloc(sizeof *threads * g_nthreads)) == NULL)
			die("Out of memory\n");

		for(int i = 0; i < g_nthreads; i++) {
			if(pthread_create(&threads[i], NULL, threadfunc, NULL))
				die("Could not start thread\n");
		}

		/* Join the threads */
		for(int i = 0; i < g_nthreads; i++) {
			if(pthread_join(threads[i], NULL))
				die("Could not join thread\n");
		}

		free(threads);
	}

	return 0;
}
