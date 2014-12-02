#include <tcp_server.h>
#include <connection.h>


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
	tcp_server srv = tcp_server_new();

	tcp_server_init(srv);
	tcp_server_set_service_function(srv, fn, NULL);
	tcp_server_get_root_resources(srv);

	tcp_server_start(srv);

	tcp_server_free(srv);
	return 0;
}
