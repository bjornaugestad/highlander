#include <stdio.h>

#include <httpcache.h>

extern const char *g_configfile;

int show_configuration(http_request req, http_response page)
{
    FILE* f = NULL;

    const char *nofile =
        " I was unable to open the configuration file. Maybe I was configured"
        " to change either user or root directory at startup? If so,"
        " the file is most likely"
        " present, but unreadable for this process. No reason to worry, though."
        ;

    (void)req;
    if (!add_page_start(page, PAGE_CONFIGFILE))
        goto err;

    if ( (f = fopen(g_configfile, "r")) == NULL) {
        if (!response_p(page, nofile))
            goto err;
    }
    else {
        char line[1024];

        if (!response_add(page, "<pre>"))
            goto err;

        while (fgets(line, sizeof line, f) != NULL) {
            if (!response_add(page, line))
                goto err;
        }

        if (!response_add(page, "</pre>"))
            goto err;

        fclose(f);
    }

    if (!add_page_end(page, NULL))
        goto err;

    return 0;

err:
    if (f)
        fclose(f);

    return HTTP_500_INTERNAL_SERVER_ERROR;
}
