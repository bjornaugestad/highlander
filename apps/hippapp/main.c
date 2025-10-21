#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <highlander.h>
#include <meta_process.h>
#include <meta_common.h>

#include "mypages.h"

int main(int argc, char *argv[])
{
	// Name of running process
	const char *appname = "foo";

	// application's root/working directory
	const char *rootdir = "/tmp";

	// application runs with this user's privileges
	const char *user = "nobody";

	http_server s;
	process proc;
	int fork_and_close = 0;

	// Bind and listen to host/port.
	static const char *hostname = "localhost";
	uint16_t portnumber = 2000;

	// Silence the compiler
	(void)argc;
	(void)argv;

	// Enable debug output from the debug() function.
	meta_enable_debug_output();

	// Print a test line.
	debug("Here we go\n");

	// First we create the web server and the process
	if( (s = http_server_new(SOCKTYPE_TCP)) == NULL)
		die("Could not create http server.\n");

	if( (proc = process_new(appname)) == NULL)
		die("Could not create process object.\n");

	// Configure the process object.
	// See functions' man pages for details.
	// Note that one must be root to change to root dir.
	if (getuid() == 0) {
		if (!process_set_rootdir(proc, rootdir))
			die("Could not change root directory\n");

		if (!process_set_username(proc, user))
			die("Could not set user name\n");
	}

	// Configure some server values. Not needed, but makes it
	// to change values later.
	http_server_set_worker_threads(s, 8);
	http_server_set_queue_size(s, 10);
	http_server_set_max_pages(s, 20);

	// Allocate all buffers needed
	if(!http_server_alloc(s)) {
		http_server_free(s);
		exit(EXIT_FAILURE);
	}

	// Add HTML pages to the server
	if (!http_server_add_page(s, "/hippapp.html", hipp_hippapp_html, NULL))
		die("Could not add page to http server.\n");

	if (!http_server_set_host(s, hostname))
		die("Out of memory.\n");

	http_server_set_port(s, portnumber);


#if 0
	http_server_set_documentroot(s, "/path/to/my/root");
	http_server_set_can_read_files(s, 1);
	http_server_set_post_limit(s, 1024 * 1024);
#endif

#if 0
	// SSL init code. Note that you need cert and key
	// That takes some manual work.
	if (!openssl_init())
		die("Could not initialize SSL library\n");
	if (!http_server_set_rootcert(s, "./rootcert.pem"))
		die("Could not set root cert\n");
	if (!http_server_set_private_key(s, "./server.pem"))
		die("Could not set private key\n");
#endif

	// Start the server from the process object.
	if (!http_server_start_via_process(proc, s))
		die("Could not add http server to process object.\n");

	if (!process_start(proc, fork_and_close))
		die("process_start failed: %s\n", strerror(errno));

	if (!process_wait_for_shutdown(proc))
		die("Failed to wait for shutdown.\n");

	// Do general cleanup
	http_server_free(s);
	process_free(proc);
	return 0;
}

