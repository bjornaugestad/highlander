#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <meta_misc.h>
#include <meta_configfile.h>
#include <meta_filecache.h>
#include <meta_list.h>
#include <highlander.h>
#include <cstring.h>

#include <httpcache.h>

/* Our file cache */
filecache g_filecache = NULL;

/* Values from the configuration file */
char g_dirs[10240];
char g_files[10240];
cstring* g_patterns = NULL;
size_t g_npatterns = 0;

#ifdef NDEBUG
const char* g_configfile = "/etc/webcached.conf";
#else
const char* g_configfile = "./webcached.conf";
#endif

static int g_daemonize = 0;

/*
 * We have finally a version of verbose() in the highlander library.
 * This variable sets the level of verbosity.
 */
extern int meta_verbose_level;

http_server g_server = NULL;
static http_server g_adminserver = NULL;

static void show_usage(FILE *out);
static void create_cache(void);
static void configure_admin_server(http_server s, const char *filename);

int main(int argc, char *argv[])
{
    process p;
    int option;

    while ( (option = getopt(argc, argv, "vhDc:")) != EOF) {
        switch (option) {
            case 'D':
                g_daemonize = 1;
                break;

            case 'v':
                meta_verbose_level++;
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

    create_cache();

    g_server = http_server_new(SOCKTYPE_TCP);
    g_adminserver = http_server_new(SOCKTYPE_TCP);
    p = process_new("webcached");

    configure_admin_server(g_adminserver, g_configfile);

    if (!http_server_configure(g_server, p, g_configfile)) {
        perror(g_configfile);
        exit(EXIT_FAILURE);
    }

    if (!http_server_alloc(g_server)) {
        perror("http_server_alloc");
        exit(EXIT_FAILURE);
    }

    http_server_set_default_page_handler(g_server, handle_requests);
    if (!http_server_start_via_process(p, g_server)
    || !http_server_start_via_process(p, g_adminserver)) {
        perror("http_server_start_via_process");
        exit(EXIT_FAILURE);
    }

    if (!process_start(p, g_daemonize))
        die_perror("process_start");

    /* NOTE TO SELF:
     * Do not start threads before the call to process_start(), as that
     * function sets up the handling of signals. Linux will send the
     * TERM signal to a randomly chosen thread unless process_start()
     * has been called. If that thread is your thread and the thread
     * doesn't block signals appropriately, the process will be
     * terminated by the OS instead of by our shutdown code.
     */
    if (!statpack_start())
        die("Could not start the statpack thread\n");

    verbose(1, "Waiting for shutdown signal(TERM)\n");
    if (!process_wait_for_shutdown(p))
        die("Failed to wait for shutdown: %s\n", strerror(errno));

    verbose(1, "Shutdown signal(TERM) received\n");

    statpack_stop();
    process_free(p);
    http_server_free(g_server);
    http_server_free(g_adminserver);
    filecache_free(g_filecache);
    cstring_multifree(g_patterns, g_npatterns);
    free(g_patterns);
    exit(0);
}

static void show_usage(FILE *out)
{
    fprintf(out, "USAGE: webcached [options]\n");
    fprintf(out, "where options can be\n");
    fprintf(out, "\t-D daemonize\n");
    fprintf(out, "\t-h Help. Prints this text\n");
    fprintf(out, "\t-c path_to_configuration_file.\n");
}

/*
 * Create a cstring array with one entry per pattern.
 * We will  use it to match file names later
 */

static void split_filename_patterns(void)
{
    if ( (g_npatterns = cstring_split(&g_patterns, g_files, " \t")) == 0)
        die("The files directive had no elements\n");
}

static void create_cache(void)
{
    configfile cf;
    unsigned long buckets, size;
    list lst;
    list_iterator li;
    unsigned long id;

    /* Read the configuration file */
    if ( (cf = configfile_read(g_configfile)) == NULL) {
        die_perror("%s", g_configfile);
    }
    else if (!configfile_get_ulong(cf, "size", &size)) {
        die("The size configuration parameter is required\n");
    }
    else if (!configfile_get_ulong(cf, "buckets", &buckets)) {
        die("The buckets configuration parameter is required\n");
    }
    else if (!configfile_get_string(cf, "files", g_files, sizeof g_files)) {
        die("The files configuration parameter is required\n");
    }
    else if (!configfile_get_string(cf, "dirs", g_dirs, sizeof g_dirs)) {
        die("The dirs configuration parameter is required\n");
    }
    else if ((g_filecache = filecache_new(size, buckets * 1024 * 1024)) == NULL) {
        die_perror("filecache_new");
    }

    configfile_free(cf);

    split_filename_patterns();

    if ( (lst = list_new()) == NULL)
        die_perror("list_new");

    verbose(1, "Checking directories for files\n");
    if (walk_all_directories(g_dirs, g_patterns, g_npatterns, lst, 1)) {
        for (li = list_first(lst); !list_end(li); li = list_next(li)) {
            fileinfo fi = list_get(li);

            if (!filecache_add(g_filecache, fi, 1, &id))
                die("Could not add object to cache\n");
        }

        verbose(1, "Cache created without errors.\n");
    }
    else {
        verbose(1, "Could not get files from the directories\n");
    }

    /* filecache now owns the fileinfo objects, we just free the list */
    sublist_free(lst);
}

static void configure_admin_server(http_server s, const char *configfilename)
{
    int port;
    char host[1024];
    configfile cf;

    assert(s != NULL);
    assert(configfilename != NULL);

    if ( (cf = configfile_read(configfilename)) == NULL)
        die_perror("%s", configfilename);

    if (!configfile_get_int(cf, "admin_port", &port))
        die_perror("admin_port");

    if (!configfile_get_string(cf, "admin_host", host, sizeof host))
        die("admin_host is missing from the configuration file %s", configfilename);

    http_server_set_port(s, port);
    if (!http_server_set_host(s, host))
        die("Out of memory. That's odd...\n");

    if (!http_server_alloc(s))
        die("Could not allocate memory for admin server");

    if (!http_server_add_page(s, "/", handle_main, NULL)
    || !http_server_add_page(s, "/index.html", handle_main, NULL)
    || !http_server_add_page(s, "/stats", show_stats, NULL)
    || !http_server_add_page(s, "/configuration", show_configuration, NULL)
    || !http_server_add_page(s, "/disk", show_disk, NULL)
    || !http_server_add_page(s, "/cache", show_cache, NULL)
    || !http_server_add_page(s, "/about", show_about, NULL)
    || !http_server_add_page(s, "/webcache_logo.gif", show_webcache_logo_gif, NULL)
    || !http_server_add_page(s, "/webcache_styles.css", show_webcache_styles_css, NULL))
        die("Coult not add pages to the admin server.");

    configfile_free(cf);
}

int handle_main(http_request req, http_response page)
{
    const char *html =
    "<p>Welcome to the Highlander web cache Administation server."
    "Here you can view statistics about the performance of the web cache,"
    " view changes on disk since the files in the cache was loaded."
    " You can also reload the cache from disk."
    "<p>The web cache is primarily controlled by the configuration file,"
    " which is read at startup."
    ;

    (void)req;

    if (add_page_start(page, PAGE_MAIN)
    && response_add(page, html)
    && add_page_end(page, NULL))
        return 0;

    return HTTP_500_INTERNAL_SERVER_ERROR;
}
