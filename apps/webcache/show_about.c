
#include <httpcache.h>

int show_about(http_request req, http_response page)
{
    const char* html =
        "<p>So what's a web cache good for anyway? The objective"
        " for this web cache is to offload high traffic web sites"
        " by moving static data to a separate web server and"
        " serve the requests for the static data as fast as possible,"
        " so that the original web server can focus on its main tasks."
        "<p>The Highlander web cache loads data from disk at startup"
        " and never reads from disk again, unless asked to do so."
        " This way the data(images, files, anything static) is always"
        " available in memory and can be sent to the client with"
        " zero disk access and zero system calls except for the network"
        " write() calls."
        "<p>The Highlander web cache is very portable between POSIX"
        " systems, meaning that you can scale both up and down as"
        " your needs changes."
        ;


    (void)req;
    if (!add_page_start(page, PAGE_ABOUT))
        return HTTP_500_INTERNAL_SERVER_ERROR;

    if (!response_add(page, html))
        return HTTP_500_INTERNAL_SERVER_ERROR;

    if (!add_page_end(page, NULL))
        return HTTP_500_INTERNAL_SERVER_ERROR;

    return 0;
}
