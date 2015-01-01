#include <assert.h>
#include <stdlib.h>
#include <meta_socket.h>

#include <highlander.h>

struct http_client_tag {
	http_request request;
	http_response response;
	connection conn;
	membuf readbuf;
	membuf writebuf;


	int timeout_reads, timeout_writes, nretries_read, nretries_write;
};

//meta_error e = meta_error_new();


http_client http_client_new(void)
{
	http_client new;

	if ((new = calloc(1, sizeof *new)) == NULL)
		return NULL;

	if ((new->request = request_new()) == NULL)
		goto memerr;

	if ((new->response = response_new()) == NULL)
		goto memerr;

	if ((new->readbuf = membuf_new(10 * 1014)) == NULL)
		goto memerr;

	if ((new->writebuf = membuf_new(10 * 1014)) == NULL)
		goto memerr;

	// Some default timeout and retry values. Later, 
	// change connection_new() to not accept these. Instead,
	// use set/get-functions.
	new->timeout_reads = new->timeout_writes = 1000;
	new->nretries_read = new->nretries_write = 5;
	new->conn = connection_new(new->timeout_reads, new->timeout_writes,
		new->nretries_read, new->nretries_write, NULL);
	if (new->conn == NULL)
		goto memerr;

	connection_assign_read_buffer(new->conn, new->readbuf);
	connection_assign_write_buffer(new->conn, new->writebuf);

	return new;

memerr:
	request_free(new->request);
	response_free(new->response);
	membuf_free(new->readbuf);
	membuf_free(new->writebuf);
	connection_free(new->conn);
	free(new);
	return NULL;
}

void http_client_free(http_client this)
{
	if (this == NULL)
		return;

	request_free(this->request);
	response_free(this->response);
	membuf_free(this->readbuf);
	membuf_free(this->writebuf);
	connection_free(this->conn);
	free(this);
}

status_t http_client_connect(http_client this, const char *host, int port)
{
	assert(this != NULL);
	assert(host != NULL);

	return connection_connect(this->conn, host, port);
}

status_t http_client_get(http_client this, const char *host, const char *uri)
{
	assert(this != NULL);

	request_set_method(this->request, METHOD_GET);
	request_set_version(this->request, VERSION_11);
	if (!request_set_host(this->request, host)
	|| !request_set_uri(this->request, uri)
	|| !request_set_user_agent(this->request, "highlander"))
		return failure;

	if (!request_send(this->request, this->conn, NULL))
		return failure;

	if (!response_receive(this->response, this->conn, 10*1024*1024, NULL))
		return failure;

	return success;

}

int http_client_http_status(http_client this)
{
	assert(this != NULL);
	return response_get_status(this->response);
}

http_response http_client_response(http_client this)
{
	assert(this != NULL);
	return this->response;
}

status_t http_client_disconnect(http_client this)
{
	assert(this != NULL);
	return connection_close(this->conn);
}

int http_client_get_timeout_write(http_client this)
{
	assert(this != NULL);
	return this->timeout_writes;
}

int http_client_get_timeout_read(http_client this)
{
	assert(this != NULL);
	return this->timeout_reads;
}

void http_client_set_timeout_write(http_client this, int millisec)
{
	assert(this != NULL);
	this->timeout_writes = millisec;
}

void http_client_set_timeout_read(http_client this, int millisec)
{
	assert(this != NULL);
	this->timeout_reads = millisec;
}

void http_client_set_retries_read(http_client this, int count)
{
	assert(this != NULL);
	this->nretries_read = count;;
}

void http_client_set_retries_write(http_client this, int count)
{
	assert(this != NULL);
	this->nretries_write = count;
}

#ifdef CHECK_HTTP_CLIENT
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	http_client p;
	http_response resp;

	const char *hostname ="www.vg.no";
	const char *uri = "/";
	int port = 80;

	if ((p = http_client_new()) == NULL)
		exit(1);

	if (!http_client_connect(p, hostname, port)) {
		fprintf(stderr, "Could not connect to %s\n", hostname);
		exit(1);
	}

	if (!http_client_get(p, hostname, uri)) {
		fprintf(stderr, "Could not get %s from %s\n", uri, hostname);
		http_client_disconnect(p);
		exit(1);
	}

	if (!http_client_disconnect(p)) {
		fprintf(stderr, "Could not disconnect from %s\n", hostname);
		exit(1);
	}

	resp = http_client_response(p);
	/* Copy some bytes from the response, just to see that we got something */
	{
		size_t n = 10, cb = response_get_content_length(resp);
		const char* s = response_get_entity(resp);

		while(n-- && cb--)
			putchar(*s++);
	 }


	http_client_free(p);
	return 0;
}
#endif

