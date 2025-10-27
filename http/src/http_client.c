#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <gensocket.h>
#include <openssl/ssl.h>

#include <highlander.h>
#include <miscssl.h>
#include <tcp_client.h>
#include <http_request.h>
#include <http_response.h>
#include <http_client.h>

struct http_client_tag {
    tcp_client tcpclient;
    http_request request;
    http_response response;
};

http_client http_client_new(int socktype)
{
    assert(socktype == SOCKTYPE_TCP || socktype == SOCKTYPE_SSL);

    http_client new = calloc(1, sizeof *new);
    if (new == NULL)
        return NULL;

    new->request = request_new();
    if (new->request == NULL)
        goto memerr;

    new->response = response_new();
    if (new->response == NULL)
        goto memerr;

    new->tcpclient = tcp_client_new(socktype);
    if (new->tcpclient == NULL)
        goto memerr;

    return new;

memerr:
    http_client_free(new);
    return NULL;
}

void http_client_free(http_client this)
{
    if (this == NULL)
        return;

    request_free(this->request);
    response_free(this->response);
    tcp_client_free(this->tcpclient);
    free(this);
}

status_t http_client_connect(http_client this, const char *host, uint16_t port)
{
    assert(this != NULL);
    assert(host != NULL);

    return tcp_client_connect(this->tcpclient, host, port);
}

status_t http_client_get(http_client this, const char *host, const char *uri, error e)
{
    assert(this != NULL);

    request_set_method(this->request, METHOD_GET);
    request_set_version(this->request, VERSION_11);

    if (!request_set_host(this->request, host)
    || !request_set_uri(this->request, uri)
    || !request_set_user_agent(this->request, "highlander"))
        return failure;

    connection c = tcp_client_connection(this->tcpclient);
    if (!request_send(this->request, c, e))
        return failure;

    if (!response_receive(this->response, c, 10*1024*1024, e))
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
    return tcp_client_close(this->tcpclient);
}

unsigned http_client_get_timeout_write(http_client this)
{
    assert(this != NULL);
    return tcp_client_get_timeout_write(this->tcpclient);
}

unsigned http_client_get_timeout_read(http_client this)
{
    assert(this != NULL);
    return tcp_client_get_timeout_read(this->tcpclient);
}

void http_client_set_timeout_write(http_client this, unsigned millisec)
{
    assert(this != NULL);
    tcp_client_set_timeout_write(this->tcpclient, millisec);
}

void http_client_set_timeout_read(http_client this, unsigned millisec)
{
    assert(this != NULL);
    tcp_client_set_timeout_read(this->tcpclient, millisec);
}

void http_client_set_retries_read(http_client this, unsigned count)
{
    assert(this != NULL);
    tcp_client_set_retries_read(this->tcpclient, count);
}

void http_client_set_retries_write(http_client this, unsigned count)
{
    assert(this != NULL);
    tcp_client_set_retries_write(this->tcpclient, count);
}

#ifdef CHECK_HTTP_CLIENT
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    error e = error_new();

    const char *hostname = "www.random.org";
    uint16_t port = 80;
    const char *uri = "/cgi-bin/randbyte?nbytes=32&format=h";

    if (!openssl_init())
        exit(1);

    http_client p = http_client_new(SOCKTYPE_TCP);
    if (p == NULL)
        die("http_client_new() returned NULL\n");

    if (!http_client_connect(p, hostname, port))
        die("Could not connect to %s\n", hostname);

    if (!http_client_get(p, hostname, uri, e)) {
        http_client_disconnect(p);
        die("Could not get %s from %s\n", uri, hostname);
    }

    int status = http_client_http_status(p);
    printf("Server returned %d\n", status);

    http_response resp = http_client_response(p);
    /* Copy some bytes from the response, just to see that we got something */
    {
        size_t n = 1000, cb = response_get_content_length(resp);
        const char *s = response_get_entity(resp);

        if (cb == 0)
            printf("Got zero bytes of content\n");

        while (n-- && cb--)
            putchar(*s++);
     }

    if (!http_client_disconnect(p))
        die("Could not disconnect from %s\n", hostname);

    http_client_free(p);
    error_free(e);
    return 0;
}
#endif
