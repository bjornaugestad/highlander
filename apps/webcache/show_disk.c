#include <stdio.h>
#include <assert.h>

#include <highlander.h>
#include <meta_filecache.h>

#include <httpcache.h>

static int show_deleted_files(http_request req, http_response page);
static int show_new_files(http_request req, http_response page);
static int show_modified_files(http_request req, http_response page);
static int show_file_summary(http_request req, http_response page);

extern char g_dirs[];
extern cstring* g_patterns;
extern size_t g_npatterns;
extern filecache g_filecache;

int show_disk(http_request req, http_response page)
{
    const char* action;
    int rc = 0;

    add_page_start(page, PAGE_DISK);

    response_href(page, "/disk?a=new", "Show new files in disk\n");
    response_br(page);
    response_href(page, "/disk?a=modified", "Show modified files in disk\n");
    response_br(page);
    response_href(page, "/disk?a=deleted", "Show files deleted from disk\n");
    response_br(page);
    response_href(page, "/disk?a=summary", "Show summary of changes\n");
    response_br(page);
    
    if ( (action = request_get_parameter_value(req, "a")) == NULL) {
        /* No action */
        add_page_end(page, NULL);
    }
    else if (strcmp(action, "deleted") == 0) {
        rc = show_deleted_files(req, page);
    }
    else if (strcmp(action, "new") == 0) {
        rc = show_new_files(req, page);
    }
    else if (strcmp(action, "modified") == 0) {
        rc = show_modified_files(req, page);
    }
    else if (strcmp(action, "summary") == 0) {
        rc = show_file_summary(req, page);
    }
    else {
        /* No action */
        add_page_end(page, NULL);
    }

    return rc;
}

static int show_deleted_files(http_request req, http_response page)
{
    int rc = HTTP_500_INTERNAL_SERVER_ERROR;
    list deleted = NULL;

    /* We lookup all deleted files from the cache to get the 
     * fileinfo data for the files. Just copy the pointer and
     * use sublist_free() to free mem.
     */
    list filist = NULL;
    list_iterator li;
    const char* msg = NULL;

    const char* desc = "These files have been deleted since the server started";
    const char* no_files = "No files have been deleted since the cache was loaded";

    (void)req;
    if ( (deleted = find_deleted_files(g_dirs, g_patterns, g_npatterns)) == NULL)
        goto cleanup;
    else if ((filist = list_new()) == NULL)
        goto cleanup;
    else {
        for (li = list_first(deleted); !list_end(li); li = list_next(li)) {
            fileinfo fi;
            const char* s = list_get(li);
            assert(s != NULL);

            if ( (fi = filecache_fileinfo(g_filecache, s)) == NULL)
                goto cleanup;
            else if (list_add(filist, fi) == NULL)
                goto cleanup;
        }

        if (list_size(filist) > 0) 
            show_file_list(page, desc, filist);
        else 
            msg = no_files;

        add_page_end(page, msg);
        rc = HTTP_200_OK;
    }

cleanup:
    list_free(deleted, NULL);
    sublist_free(filist);

    return rc;
}

static int show_new_files(http_request req, http_response page)
{
    int rc = HTTP_500_INTERNAL_SERVER_ERROR; /* Default return in case of errors */

    const char* msg = NULL;
    const char* desc = 
        "Below is a list all the files on disk that match the files pattern"
        " from the configuration file and aren't already in the cache";

    const char* no_files = "No files have been added on disk since the cache was loaded";

    list files = NULL;

    (void)req;
    if ( (files = find_new_files(g_dirs, g_patterns, g_npatterns)) == NULL)
        goto cleanup;
    else {
        if (list_size(files) == 0) 
            msg = no_files;
        else if (!show_file_list(page, desc, files))
            goto cleanup;

        add_page_end(page, msg);
        rc = HTTP_200_OK;
    }

cleanup:
    list_free(files, (dtor)fileinfo_free);
    return rc;
}


static int show_modified_files(http_request req, http_response page)
{
    int rc = HTTP_500_INTERNAL_SERVER_ERROR; /* Default return in case of errors */

    const char* msg = NULL;
    const char* desc = 
        "Below is a list all the files on disk that match the files pattern"
        " from the configuration file and have been modified on disk"
        " since the server started.";

    const char* no_files = 
        "No files in cache have been modified on disk after the cache was loaded";

    list files = NULL;

    (void)req;
    if ( (files = find_modified_files(g_dirs, g_patterns, g_npatterns)) == NULL)
        goto cleanup;
    else {
        if (list_size(files) == 0) 
            msg = no_files;
        else 
            show_file_list(page, desc, files);

        add_page_end(page, msg);
        rc = HTTP_200_OK;
    }

cleanup:
    list_free(files, (dtor)fileinfo_free);
    return rc;
}


static int show_file_summary(http_request req, http_response page)
{
    list del = NULL, mod = NULL, new = NULL;
    size_t ndel = 0, nmod = 0, nnew = 0;
    int rc = HTTP_500_INTERNAL_SERVER_ERROR;

    (void)req;

    if ( (mod = find_modified_files(g_dirs, g_patterns, g_npatterns)) == NULL)
        goto cleanup;
    else if ((new = find_new_files(g_dirs, g_patterns, g_npatterns)) == NULL)
        goto cleanup;
    else if ((del = find_deleted_files(g_dirs, g_patterns, g_npatterns)) == NULL)
        goto cleanup;

    ndel = list_size(del);
    nmod = list_size(mod);
    nnew = list_size(new);

    response_add(page, "<table columns=2><th>Text</th><th>Count</th>\n");
    response_printf(page, 100, "<tr><td>New files</td><td>%zu</td>\n", nnew);
    response_printf(page, 100, "<tr><td>Modified files</td><td>%zu</td>\n", nmod);
    response_printf(page, 100, "<tr><td>Deleted files</td><td>%zu</td>\n", ndel);
    response_add(page, "</table>");
    add_page_end(page, NULL);
    rc = HTTP_200_OK;
    /* Fallthrough is indended */

cleanup:
    list_free(mod, (dtor)fileinfo_free);
    list_free(del, (dtor)NULL);
    list_free(new, (dtor)fileinfo_free);
    return rc;
}
