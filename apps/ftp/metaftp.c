#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <meta_list.h>
#include <meta_misc.h>
#include <highlander.h>
#include <rfc1738.h>

#include "pagehandlers.h"

/*
 * Paths are important in this application. We handle
 * absolute paths to both directories and files, as well as
 * paths relative to the root directory of the running process.
 * UNIX supports multiple file system types, each type can have
 * different path length limits (man pathconf(3)), and 
 * file systems can be mounted and unmounted while we are running.
 * If we want to be 100% politically correct, we should call
 * pathconf() , malloc() and free() every time we work with a path,
 * but that would make our application awfully slow. 
 * Instead we allocate our paths on the stack as a char array and
 * ALWAYS test for lengths and overflow.
 */
#define FTP_PATH_MAX 10240
typedef char path_t[FTP_PATH_MAX + 1];

/*
 * We store directory info here.
 */
struct dirinfo {
    struct stat st;
    path_t name;
};

static int show_directory(http_response page, const path_t abspath, const char *uripath);
static list read_directory(const path_t abspath);
static int add_html_header(http_response page, const char *path);
static int add_html_footer(http_response page);

/**
 * Creates a full path from root+relative path.
 * Will allocate space for the string, so please free it when done with it.
 * relpath can be "/", but not "" or NULL.
 */
static int makepath(path_t abspath, const char *relpath);

/*
 * This is our callback function, called from highlander whenever 
 * the http_server gets a request for a page. This process has NO
 * pages, so the default page handler, this function, is called instead.
 * 
 * This function accepts any URI and tries to map that URI to a file
 * or directory relative to the root directory of the process. If
 * the URI refers to a directory, the contents of that directory is 
 * listed. If the URI refers to a file, the file will be sent to the 
 * client.
 */
static int handle_requests(const http_request req, http_response page)
{
    const char* s;
    path_t uri, abspath;
    struct stat st;
    int rc = HTTP_200_OK;

    /* Check that client uses correct request type */
    if (request_get_method(req) != METHOD_GET
    || request_get_parameter_count(req) > 0) {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if ( (s = request_get_uri(req)) == NULL || strlen(s) == 0) {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (!rfc1738_decode_string(uri, sizeof uri, s)) {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (uri[0] != '/') {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (strstr(uri, "..") != NULL) {
        rc = HTTP_400_BAD_REQUEST;
    }
    else if (!makepath(abspath, uri)) {
        rc = HTTP_500_INTERNAL_SERVER_ERROR;
    }
    else if (stat(abspath, &st)) {
        size_t cb = strlen(uri) + strlen(abspath) + 100;
        response_printf(page, cb, "%s(%s): Not found", uri, abspath);
        rc = HTTP_404_NOT_FOUND;
    }
    else if (S_ISDIR(st.st_mode)) {
        if (show_directory(page, abspath, &uri[1]))  /* Remove leading / */
            rc = HTTP_200_OK;
        else
            rc = HTTP_500_INTERNAL_SERVER_ERROR;
    }
    else if (!S_ISREG(st.st_mode)) {
        rc = HTTP_500_INTERNAL_SERVER_ERROR;
    }
    else if (!response_send_file(page, abspath, get_mime_type(abspath), NULL)) {
        rc = HTTP_500_INTERNAL_SERVER_ERROR;
    }
    else  
        rc = HTTP_200_OK;
    
    return rc;
}

#ifdef NDEBUG
/* Release version */
static const char* g_configfile = "/etc/metaftp.conf";
#else
/* Debug version */
static const char* g_configfile = "./metaftp.conf";
#endif

static int g_daemonize = 0;
static http_server g_server = NULL;

static void show_usage(FILE *out);

int main(int argc, char *argv[])
{
    process p;
    int option;

    while ( (option = getopt(argc, argv, "hDc:")) != EOF) {
        switch (option) {
            case 'D':
                g_daemonize = 1;
                break;

            case 'c':
                g_configfile = optarg;
                break;

            case 'h':
                show_usage(stdout);
                exit(EXIT_SUCCESS);

            default:
                show_usage(stderr);
                exit(EXIT_FAILURE);
        }
    }

    g_server = http_server_new();
    p = process_new("metaftp");

    if (!http_server_configure(g_server, p, g_configfile)) {
        perror(g_configfile);
        exit(EXIT_FAILURE);
    }

    if (!http_server_alloc(g_server)) {
        perror("http_server_alloc");
        exit(EXIT_FAILURE);
    }
    http_server_add_page(g_server, "/folder.gif", show_folder_gif, NULL);
    http_server_add_page(g_server, "/document.png", show_document_png, NULL);
    http_server_add_page(g_server, "/about.html", show_about_html, NULL);
    http_server_set_default_page_handler(g_server, handle_requests);

    if (!http_server_start_via_process(p, g_server)) {
        perror("http_server_start_via_process");
        exit(EXIT_FAILURE);
    }

    if (!process_start(p, g_daemonize)) {
        perror("process_start");
        exit(EXIT_FAILURE);
    }

    process_wait_for_shutdown(p);
    process_free(p);
    http_server_free(g_server);
    exit(0);
}

static int sort_directory(const void *p1, const void *p2)
{
    const struct dirinfo *s1 = p1, *s2 = p2;

    /* We always sort directories first */
    if (S_ISDIR(s1->st.st_mode) && !S_ISDIR(s2->st.st_mode))
        return -1;
    else if (!S_ISDIR(s1->st.st_mode) && S_ISDIR(s2->st.st_mode))
        return 1;

    /* Both are directories or neither are, sort by filename */
    return strcmp(s1->name, s2->name);
}

static int show_directory_as_html(http_response page, list lst, const char *uri)
{
    list_iterator i;
    struct tm t;
    int cb;

    assert(lst != NULL);

    if (lst == NULL)
        return 0;

    /* Modify uri if we point to root directory */
    if (strlen(uri) == 0)
        uri = "/";

    if (!add_html_header(page, uri))
        return 0;

    for (i = list_first(lst); !list_end(i); i = list_next(i)) {
        path_t _link = { '\0' }, encoded_link = { '\0' };
        struct dirinfo *p = list_get(i);
        a2p(page, "<tr>\n");

        /* Create _link, which is uri + name */
        cb = snprintf(_link, sizeof _link, "%s%s%s",
            uri, 
            strcmp(uri, "/") == 0 ? "" : "/",
            p->name);

        if (cb > (int)sizeof _link)
            return 0;

        if (!rfc1738_encode_string(encoded_link, sizeof encoded_link, _link))
            return 0;

        if (S_ISDIR(p->st.st_mode))
            response_add(page, "<td><img align='middle' border=0 src='/folder.gif'>");
        else
            response_add(page, "<td><img align='middle' border=0 src='/document.png'>");

        /* How many bytes do we need to print this? */
        if ( (cb = snprintf(NULL, 0, "<a href='%s'>%s</a></td>\n", encoded_link, p->name)) < 0)
            return 0;

        response_printf(page, cb + 4, "<a href='%s'>%s</a></td>\n", encoded_link, p->name);
        response_printf(page, 100, "<td align='right'>%lu</td>", p->st.st_size);

        if (gmtime_r(&p->st.st_mtime, &t) != NULL) {
            char sz[1024];
            strftime(sz, sizeof(sz), "%d/%m/%Y %H:%M:%S GMT", &t);
            response_td(page, sz);
        }

        response_add(page, "</tr>\n");
    }

    return add_html_footer(page);
}

static void dirinfo_dtor(void *node)
{
    struct dirinfo *p = node;

    if (p != NULL) {
        free(p);
    }
}

/* Return 1 on success and 0 on error */
int show_directory(http_response page, const path_t abspath, const char *uri)
{
    list lst;
    int success = 1;

    if ( (lst = read_directory(abspath)) == NULL)
        success = 0;
    else {
        list_sort(lst, sort_directory);
        success = show_directory_as_html(page, lst, uri);
    }

    list_free(lst, dirinfo_dtor);
    return success;
}

static int concat_paths(char* dest, size_t cb, const char* p1, const char* p2)
{
    size_t cbp1 = strlen(p1);
    if (cbp1 + strlen(p2) + 2 > cb)
        return 0;

    strcpy(dest, p1);
    if (p1[cbp1 - 1] != '/' && *p2 != '/')
        strcat(dest, "/");
    strcat(dest, p2);

    return 1;
}

/* Stats the path and adds path+statinfo to list.
 * Again we do not know the path_max for the file system so
 * we must check.
 *
 * Returns 1 on success or 0 if we fail.
 * abspath is the absolute path to "current directory".
 * filename is the name of file/dir we are about to stat/add.
 *
 * Note: We only handle directories and regular files.
 */
static int add_entry(list lst, const path_t abspath, const char *filename)
{
    size_t cb;
    struct dirinfo *pdi;
    struct stat st;

    /* We stat() the file to get info about it 
     * and path2file is a temp variable used to store
     * the absolute path to the file. We do not want to
     * malloc()/free() this all the time, instead we
     * limit ourselves to a reasonable path length.
     * Few browsers support 10K uris anyway and no users
     * want them.
     */
    path_t path2file;

    /* Illegal parameters are rejected */
    if (lst == NULL || abspath == NULL || filename == NULL)
        return 0;

    /* Empty paths cannot be added */
    if (strlen(abspath) == 0 || strlen(filename) == 0)
        return 0;

    /* Only absolute abspath directories can be added */
    if (*abspath != '/')
        return 0;

    /* The . and .. should not be added */
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
        return 0;

    /* add 2, one for / and one for '\0' */
    cb = strlen(abspath) + strlen(filename) + 2;
    if (cb > sizeof path2file)
        return 0;

    /* Now create an absolute path to file and stat() the file */
    if (!concat_paths(path2file, sizeof path2file, abspath, filename))
        return 0;

    if (stat(path2file, &st))
        return 0;

    /* Return 1 to continue processing even if we didn't add a node */
    if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode))
        return 1;

    if ( (pdi = malloc(sizeof *pdi)) == NULL)
        return 0;

    pdi->st = st;
    strcpy(pdi->name, filename);
    if (list_add(lst, pdi) == NULL) {
        dirinfo_dtor(pdi);
        return 0;
    }

    return 1;
}

/*
 * Store all entries in a list so that we can sort and filter later.
 * debuff (Directory entry buffer) is used by readdir_r to store
 * data. It must be large enough to hold the struct dirent plus
 * the longest path allowed. The path varies with the file system,
 * as different file systems have different limits. We must therefore
 * call it every time as we don't know if other file systems gets 
 * mounted while we are running.
 *
 * Function returns NULL if an error occurs, else a list which
 * must be freed after use.
 */
list read_directory(const path_t abspath)
{
    DIR *d = NULL;
    struct dirent *de = NULL;
    char debuff[sizeof(struct dirent) + 100];
    int err = 0;
    list lst;

    if ((lst = list_new()) == NULL) {
        err = 1;
    }
    else if ((d = opendir(abspath)) == NULL) {
        err = 1;
    }
    else {
        while (readdir_r(d, (struct dirent*)debuff, &de) == 0 && de != NULL) {
            if (strcmp(de->d_name, ".") == 0
            || strcmp(de->d_name, "..") == 0) 
                ; /* Skip these */
            else if (!add_entry(lst, abspath, de->d_name)) {
                err = 1;
                break;
            }
        }

        closedir(d);
    }

    if (err) {
        list_free(lst, dirinfo_dtor);
        return NULL;
    }

    return lst;
}

/* Returns 1 on success, 0 on failure */
int add_html_header(http_response page, const char *uri)
{
    const char *header =
        "<html>\n"
        "	<head>\n"
        "		<title>metaftp HTTP-based ftp server</title>"
        "	</head>\n"
        "<body>\n"
        "	<h1>Index of %s</h1>\n"
        "	<hr>"
        "	<table rules='cols' cellpadding=3>\n"
        "	<th>Name</th><th>Size</th><th>Modified</th>\n"
        ;

    size_t cb;

    if (page == NULL || uri == NULL || strlen(uri) == 0)
        return 0;
    
    cb = strlen(header) + strlen(uri) + 10;
    return response_printf(page, cb, header, uri);
}

/* Returns 1 on success, 0 on failure */
int add_html_footer(http_response page)
{
    static const char *footer = 
        "</table>\n"
        "<hr>\n"
        "<address>"
        "	<a href='/about.html'>"
        "		About the Highlander HTTP-based ftp server"
        "	</a>"
        "</address>\n"
        "</body>\n"
        "</html>\n";

    return response_add(page, footer);
}

static int makepath(path_t abspath, const char *relpath)
{
    size_t cb;
    const char* docdir = http_server_get_documentroot(g_server);

    if (relpath == NULL || strlen(relpath) == 0 || *relpath != '/')
        return 0;

    cb = strlen(docdir) + strlen(relpath) + 2;
    if (cb >= FTP_PATH_MAX)
        return 0;
    else 
        return concat_paths(abspath, FTP_PATH_MAX, docdir, relpath);
}

static void show_usage(FILE *out)
{
    fprintf(out, "USAGE: metaftp [options]\n");
    fprintf(out, "where options can be\n");
    fprintf(out, "\t-D daemonize\n");
    fprintf(out, "\t-h Help. Prints this text\n");
    fprintf(out, "\t-c path_to_configuration_file.\n");
}
