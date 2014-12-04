#include <stdio.h>

#include <tcp_server.h>
#include <connection.h>
#include <meta_process.h>


static void* fn(void* arg)
{
	connection c = arg;
	char buf[1024];

	while (connection_gets(c, buf, sizeof buf)) {
		connection_puts(c, buf);
		connection_flush(c);
	}

	return NULL;
}

int main(void)
{
	process p = process_new("echoserver");
	tcp_server srv = tcp_server_new();

	tcp_server_init(srv);
	tcp_server_set_service_function(srv, fn, NULL);
	tcp_server_start_via_process(p, srv);

	if (!process_start(p, 1))
		exit(1);

	process_wait_for_shutdown(p);
	tcp_server_free(srv);
	process_free(p);
	return 0;
}
