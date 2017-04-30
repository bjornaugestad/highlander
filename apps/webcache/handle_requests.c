#include <sys/types.h>
#include <sys/stat.h>

#include <meta_filecache.h>
#include <meta_atomic.h>

#include <httpcache.h>

extern filecache g_filecache;

/* Our counters */
atomic_u32 sum_requests = ATOMIC_INITIALIZER;
atomic_ull sum_bytes = ATOMIC_INITIALIZER;
atomic_u32 sum_200 = ATOMIC_INITIALIZER;
atomic_u32 sum_404 = ATOMIC_INITIALIZER;
atomic_u32 sum_400 = ATOMIC_INITIALIZER;
atomic_u32 sum_304 = ATOMIC_INITIALIZER;
atomic_u32 sum_500 = ATOMIC_INITIALIZER;


static inline void inc_status_counter(int rc)
{
    switch (rc) {
        case HTTP_200_OK:
            atomic_u32_inc(&sum_200);
            break;
        case HTTP_304_NOT_MODIFIED:
            atomic_u32_inc(&sum_304);
            break;
        case HTTP_400_BAD_REQUEST:
            atomic_u32_inc(&sum_400);
            break;

        case HTTP_404_NOT_FOUND:
            atomic_u32_inc(&sum_404);
            break;
        case HTTP_500_INTERNAL_SERVER_ERROR:
            atomic_u32_inc(&sum_500);
            break;
        default:
            abort();
    }
}

/*
 * This is our callback function, called from highlander whenever 
 * the http_server gets a request for a page. 
 *
 * 
 * This function accepts any URI and tries to map that URI to a file
 * already in the cache. Only GET is OK and we never want params.
 * We always need an URI, / does not default to anything.
 */
int handle_requests(const http_request req, http_response page)
{
    const char *uri = NULL;
    int rc = HTTP_500_INTERNAL_SERVER_ERROR;
    void* file;
    size_t size = 0;
    struct stat st;
    time_t if_modified_since;
    char mimetype[128];

    /* Check that client uses correct request type */
    if (request_get_method(req) != METHOD_GET) {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (request_get_parameter_count(req) > 0) {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if ((uri = request_get_uri(req)) == NULL || strlen(uri) <= 1)  {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (*uri++ != '/') { /* We intentionally skip the '/' in the URI */
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (strstr(uri, "..") != NULL) {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (!filecache_get(g_filecache, uri, &file, &size)) {
        rc = HTTP_404_NOT_FOUND;
    }
    else if (!filecache_get_mime_type(g_filecache, uri, mimetype, sizeof mimetype)) {
        rc = HTTP_500_INTERNAL_SERVER_ERROR;
    }
    else if (!response_set_content_type(page, mimetype)) {
        rc = HTTP_500_INTERNAL_SERVER_ERROR;
    }
    else  if(!filecache_stat(g_filecache, uri, &st)) {
        rc = HTTP_500_INTERNAL_SERVER_ERROR;
    }
    else if ((if_modified_since = request_get_if_modified_since(req)) == (time_t)-1) {
        /* Just send it */
        response_set_last_modified(page, st.st_mtime);
        response_set_content_buffer(page, file, size);
        rc = HTTP_200_OK;
    }
    else if (if_modified_since >= st.st_mtime) {
        rc = HTTP_304_NOT_MODIFIED;
    }
    else {
        response_set_content_buffer(page, file, size);
        rc = HTTP_200_OK;
    }

    inc_status_counter(rc);
    atomic_u32_inc(&sum_requests);
    atomic_ull_add(&sum_bytes, size);

    if (uri == NULL)
        verbose(2, "Returning %d for page request with unknown URL\n", rc);
    else
        verbose(2, "Returning %d for page request for URL %s\n", rc, uri);

    return rc;
}
