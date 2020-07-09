#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <gensocket.h>
#include <openssl/ssl.h>

#include <highlander.h>
#include <miscssl.h>
#include <tcp_client.h>

struct http_client_tag {
    tcp_client sock;
    http_request request;
    http_response response;
};

http_client http_client_new(int socktype)
{
    http_client new;

    assert(socktype == SOCKTYPE_TCP || socktype == SOCKTYPE_SSL);

    if ((new = calloc(1, sizeof *new)) == NULL)
        return NULL;

    if ((new->request = request_new()) == NULL)
        goto memerr;

    if ((new->response = response_new()) == NULL)
        goto memerr;

    if ((new->sock = tcp_client_new(socktype)) == NULL)
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
    tcp_client_free(this->sock);
    free(this);
}

status_t http_client_connect(http_client this, const char *host, int port)
{
    assert(this != NULL);
    assert(host != NULL);

    return tcp_client_connect(this->sock, host, port);
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

    connection c = tcp_client_connection(this->sock);
    if (!request_send(this->request, c, NULL))
        return failure;

    if (!response_receive(this->response, c, 10*1024*1024, NULL))
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
    return tcp_client_close(this->sock);
}

int http_client_get_timeout_write(http_client this)
{
    assert(this != NULL);
    return tcp_client_get_timeout_write(this->sock); // this->timeout_writes;
}

int http_client_get_timeout_read(http_client this)
{
    assert(this != NULL);
    return tcp_client_get_timeout_read(this->sock); // this->timeout_reads;
}

void http_client_set_timeout_write(http_client this, int millisec)
{
    assert(this != NULL);
    tcp_client_set_timeout_write(this->sock, millisec);
}

void http_client_set_timeout_read(http_client this, int millisec)
{
    assert(this != NULL);
    tcp_client_set_timeout_read(this->sock, millisec);
}

void http_client_set_retries_read(http_client this, int count)
{
    assert(this != NULL);
    tcp_client_set_retries_read(this->sock, count);
}

void http_client_set_retries_write(http_client this, int count)
{
    assert(this != NULL);
    tcp_client_set_retries_write(this->sock, count);
}

#ifdef CHECK_HTTP_CLIENT
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    http_client p;
    http_response resp;

    int socktype = SOCKTYPE_SSL;
    const char *hostname = "www.random.org";
    int port = 443;
    const char *uri = "/cgi-bin/randbyte?nbytes=32&format=h";
    if (!openssl_init())
        exit(1);

    if ((p = http_client_new(socktype)) == NULL)
        die("http_client_new() returned NULL\n");

    if (!http_client_connect(p, hostname, port))
        die("Could not connect to %s\n", hostname);

    if (!http_client_get(p, hostname, uri)) {
        http_client_disconnect(p);
        die("Could not get %s from %s\n", uri, hostname);
    }

    int status = http_client_http_status(p);
    printf("Server returned %d\n", status);

    resp = http_client_response(p);
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
    return 0;
}
#endif
