#include <stdio.h>
#include <assert.h>

#include <highlander.h>
#include <meta_filecache.h>

#include <httpcache.h>

extern char g_dirs[];
extern cstring* g_patterns;
extern size_t g_npatterns;
extern filecache g_filecache;


static size_t reload_cache(void)
{
    size_t files = 0;
    list lst;

    if ( (lst = list_new()) == NULL)
        goto err;
    else if(!walk_all_directories(g_dirs, g_patterns, g_npatterns, lst, 1)) 
        goto err;
    else if(!filecache_invalidate(g_filecache)) 
        goto err;
    else {
        list_iterator li;
        unsigned long id;
        for (li = list_first(lst); !list_end(li); li = list_next(li)) {
            fileinfo fi = list_get(li);

            /* Now filecache owns the fileinfo objects */
            filecache_add(g_filecache, fi, 1, &id);
            files++;
        }
    }

    sublist_free(lst);
    return files;

err:
    list_free(lst, (dtor)fileinfo_free);
    return 0;
}

int show_cache(http_request req, http_response page)
{
    char msgbuf[100] = { '\0' };

    const char* action;
    add_page_start(page, PAGE_CACHE);
    response_href(page, "/cache?a=reload", "reload cache");
    response_br(page);

    if ( (action = request_get_parameter_value(req, "a")) == NULL) {
        /* No action */
    }
    else if(strcmp(action, "reload") == 0) {
        size_t files;
        if ( (files = reload_cache()) == 0) 
            strcpy(msgbuf, "No files were added to the cache, an error probably occured");
        else {
            sprintf(msgbuf, "Added %zu files to cache", files);
        }
    }

    add_page_end(page, msgbuf);
    return HTTP_200_OK;
}

