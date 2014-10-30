#include <stdio.h>

#include <httpcache.h>

extern const char* g_configfile;

int show_configuration(http_request req, http_response page)
{
    FILE* f;
    const char* nofile =
        " I was unable to open the configuration file. Maybe I was configured"
        " to change either user or root directory at startup? If so, the file is most likely"
        " present, but unreadable for this process. No reason to worry, though."
        ;

    (void)req;
    add_page_start(page, PAGE_CONFIGFILE);

    if ( (f = fopen(g_configfile, "r")) == NULL) {
        response_p(page, nofile);
    }
    else {
        char line[1024];
        response_add(page, "<pre>");
        while (fgets(line, sizeof line, f) != NULL) {
            response_add(page, line);
        }

        response_add(page, "</pre>");
        fclose(f);
    }

    add_page_end(page, NULL);
    return 0;
}

