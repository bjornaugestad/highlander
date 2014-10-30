 #include <highlander.h>

 int page_handler(http_request req, http_response page)
 {
    const char* html =
    "<html><head><title>Hello, world</title></head>"
    "<body>Hello, world</body></html>";

    (void)req;

    response_add(page, html);
    return 0;
 }

 int main(void)
 {
    http_server s = http_server_new();
    http_server_set_port(s, 2000);
    http_server_alloc(s);
    http_server_add_page(s, "/", page_handler, NULL);
    http_server_get_root_resources(s);
    http_server_start(s);
    return 0;
 }

