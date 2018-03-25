#include <highlander.h>

// Simple page-serving httpd

int main(void)
{
    http_server s = http_server_new(SOCKTYPE_TCP);

    http_server_free(s);
    return 0;
}
