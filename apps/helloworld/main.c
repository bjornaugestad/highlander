#include <stdio.h>
#include <highlander.h>

int page_handler(http_request req, http_response page)
{
    const char* html =
        "<html><head><title>Hello, world</title></head>"
        "<body>Hello, world</body></html>";

    (void)req;

    if (!response_add(page, html))
        return HTTP_500_INTERNAL_SERVER_ERROR;

   return 0;
}

int main(void)
{
    http_server s = http_server_new();
    http_server_set_port(s, 2000);
    if (!http_server_alloc(s))
        die("Could not allocate resources\n");

    if (!http_server_add_page(s, "/", page_handler, NULL))
        die("Coult not add page.\n");

    if (!http_server_get_root_resources(s))
        die("Could not get root resources\n");

    if (!http_server_start(s))
        die("An error occured\n");

    return 0;
}
