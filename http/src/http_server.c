/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <tcp_server.h>

#include <meta_pool.h>
#include <meta_configfile.h>
#include <meta_misc.h>
#include "internals.h"

/*
 * NOTE:
 * Husk at vi kan ha URL requests som skal mappes direkte til
 * f. eks. 301 Moved Permanently. Disse bør mappes i main()
 * før oppstart av http_server. Vi har 2 grupper av disse
 * a) Flyttet og ny adresse er kjent (301)
 * b) Borte uten ny, kjent adresse (410)
 */

/* Each server need a lot of stuff... */
struct http_server_tag {
    tcp_server tcpsrv;

    /* Should requests defer reading of posted content or not? */
    int defered_read;

    /* How many pages do we have space for */
    size_t max_pages;

    /* How many pages have been added? */
    size_t npages;

    /* Here we store the pages */
    dynamic_page* pages;

    /* attributes for pages
     * The rules are:
     * If page_attributes[n] == NULL, use default_attributes
     * If default_attributes == NULL, anything goes
     */
    page_attribute default_attributes;

    /* Shutdown flag for this server */
    int shutting_down;

    /* each server has a pool of request- and response-objects */
    pool requests;
    pool responses;

    int timeout_read;
    int timeout_write;
    int timeout_accept;
    int retries_read;
    int retries_write;

    char *host;
    int port;
    size_t worker_threads;	/* Number of threads available */
    size_t queue_size;	/* Max entries in the queue */
    int block_when_full;

    FILE *logfile;			/* File we log to */
    char logfile_name[LOGFILE_MAX + 1]; /* logfile name */
    int logrotate;			/* How often do we rotate logs, 0 == never */

    pthread_mutex_t logfile_lock;	/* Used when rotating */
    int logfile_entries;			/* Current number of entries */
    int logging;					/* Are we logging or not */

    /* The document root */
    char documentroot[DOCUMENTROOT_MAX + 1];

    /* The default page handler */
    PAGE_FUNCTION default_handler;

    /* Do we permit the web server to load files from disk or not? */
    int can_read_files;

    /* The max number of bytes a POST request can contain */
    size_t post_limit;


    int tracelevel;
};

static void set_server_defaults(http_server this)
{
    /* Set some defaults */
    this->timeout_read = 5000;
    this->timeout_write = 500;
    this->timeout_accept = 5000;
    this->max_pages = 100;
    this->npages = 0;
    this->host = NULL;
    this->port = 80;
    this->worker_threads = 8;
    this->queue_size = 100;
    this->block_when_full = 0;
    this->default_handler = NULL;
    this->retries_read = 0;
    this->retries_write = 3;

    this->logfile = NULL;
    this->logrotate = 0;
    this->logfile_entries = 0;
    this->logfile_name[0] = '\0';
    this->logging = 0;
    this->can_read_files = 0;
    strcpy(this->documentroot, "./");

    pthread_mutex_init(&this->logfile_lock, NULL);

    /* Default post limit */
    this->post_limit = 102400; /* 100 KB */
    this->defered_read = 0;
}

http_server http_server_new(int socktype)
{
    http_server this;

    if ((this = calloc(1, sizeof *this)) == NULL)
        return NULL;

    if ((this->tcpsrv = tcp_server_new(socktype)) == NULL) {
        free(this);
        return NULL;
    }

    set_server_defaults(this);
    return this;
}

void http_server_set_post_limit(http_server this, size_t cb)
{
    assert(this != NULL);
    this->post_limit = cb;
}

size_t http_server_get_post_limit(http_server this)
{
    assert(this != NULL);
    return this->post_limit;
}

void http_server_set_defered_read(http_server this, int flag)
{
    assert(this != NULL);
    this->defered_read = flag;
}

int http_server_get_defered_read(http_server this)
{
    assert(this != NULL);
    return this->defered_read;
}

status_t http_server_set_documentroot(http_server this, const char *docroot)
{
    size_t len;

    assert(this != NULL);
    assert(docroot != NULL);

    len = strlen(docroot) + 1;
    if (len > sizeof this->documentroot)
        return fail(ENOSPC);

    memcpy(this->documentroot, docroot, len);
    return success;
}

const char* http_server_get_documentroot(http_server this)
{
    assert(this != NULL);
    return this->documentroot;
}

void http_server_trace(http_server this, int level)
{
    assert(this != NULL);
    this->tracelevel = level;
}

void http_server_set_can_read_files(http_server this, int val)
{
    assert(this != NULL);
    this->can_read_files = val;
}

int http_server_can_read_files(http_server this)
{
    assert(this != NULL);
    return this->can_read_files;
}

void http_server_set_logrotate(http_server this, int logrotate)
{
    assert(this != NULL);
    assert(logrotate >= 0);

    pthread_mutex_lock(&this->logfile_lock);
    this->logrotate = logrotate;
    pthread_mutex_unlock(&this->logfile_lock);
}

static void http_server_free_page_structs(http_server this)
{
    size_t i;

    for (i = 0; i < this->npages; i++)
        dynamic_free(this->pages[i]);

    free(this->pages);
}

static void http_server_free_request_pool(http_server this)
{
    assert(this != NULL);

    pool_free(this->requests, (dtor)request_free);
    this->requests = NULL;
}

static void http_server_free_response_pool(http_server this)
{
    assert(this != NULL);

    pool_free(this->responses, (dtor)response_free);
    this->responses = NULL;
}

void http_server_free(http_server this)
{
    if (this != NULL) {
        pthread_mutex_destroy(&this->logfile_lock);
        tcp_server_free(this->tcpsrv);
        http_server_free_request_pool(this);
        http_server_free_response_pool(this);
        http_server_free_page_structs(this);

        attribute_free(this->default_attributes);
        free(this->host);

        if (this->logfile != NULL)
            fclose(this->logfile);

        free(this);
    }
}

static status_t http_server_alloc_page_structs(http_server this)
{
    if ((this->pages = calloc(this->max_pages, sizeof *this->pages)) == NULL)
        return failure;

    return success;
}

void http_server_set_default_page_handler(http_server this, PAGE_FUNCTION pf)
{
    assert(this != NULL);
    this->default_handler = pf;
}

static status_t http_server_alloc_request_pool(http_server this)
{
    size_t i;
    http_request r;

    assert(this != NULL);
    assert(this->requests == NULL);

    this->requests = pool_new(this->worker_threads);
    if (NULL == this->requests)
        return failure;

    /* Allocate each request object */
    for (i = 0; i < this->worker_threads; i++) {
        if ((r = request_new()) == NULL) {
            /* Free any prev. allocated */
            pool_free(this->requests, (dtor)request_free);
            this->requests = NULL;
            return failure;
        }

        pool_add(this->requests, r);
    }

    return success;
}

static status_t http_server_alloc_response_pool(http_server this)
{
    size_t i;
    http_response r;

    assert(this != NULL);
    assert(this->responses == NULL);

    /* Allocate main pointer */
    if ((this->responses = pool_new(this->worker_threads)) == NULL)
        return failure;

    /* Allocate each response object */
    for (i = 0; i < this->worker_threads; i++) {
        if ((r = response_new()) == NULL) {
            /* Free any prev. allocated */
            pool_free(this->responses, (dtor)response_free);
            this->responses = NULL;
            return failure;
        }

        pool_add(this->responses, r);
    }

    return success;
}

status_t http_server_alloc(http_server this)
{
    if (!http_server_alloc_page_structs(this))
        return failure;

    if (!http_server_alloc_request_pool(this)) {
        http_server_free_page_structs(this);
        return failure;
    }

    if (!http_server_alloc_response_pool(this)) {
        http_server_free_request_pool(this);
        http_server_free_page_structs(this);
        return failure;
    }

    return success;
}

static status_t configure_tcp_server(http_server this)
{
    if (!tcp_server_set_hostname(this->tcpsrv, this->host))
        return failure;

    tcp_server_set_port(this->tcpsrv, this->port);
    tcp_server_set_timeout(this->tcpsrv, this->timeout_read, this->timeout_write, this->timeout_accept);
    tcp_server_set_retries(this->tcpsrv, this->retries_read, this->retries_write);
    tcp_server_set_queue_size(this->tcpsrv, this->queue_size);
    tcp_server_set_block_when_full(this->tcpsrv, this->block_when_full);
    tcp_server_set_worker_threads(this->tcpsrv, this->worker_threads);
    tcp_server_set_service_function(this->tcpsrv, serviceConnection, this);

    return success;
}

/*
 * Start running the server. This code has changed a bit after the
 * addition of the process object with process_start...().
 * This breaks older client code unless that code calls
 * http_server_get_root_resources() right before
 * the call to http_server_go().
 */
status_t http_server_start(http_server this)
{
    if (!tcp_server_init(this->tcpsrv))
        return failure;

    if (!tcp_server_start(this->tcpsrv))
        return failure;

    return success;
}

status_t http_server_add_page(
    http_server this,
    const char *uri,
    PAGE_FUNCTION func,
    page_attribute attr)
{
    dynamic_page dp;

    assert(this != NULL);
    assert(uri != NULL);
    assert(func != NULL);
    assert(this->npages < this->max_pages);

    if ((dp = dynamic_new(uri, func, attr)) == NULL)
        return failure;

    this->pages[this->npages++] = dp;
    return success;
}

dynamic_page http_server_lookup(http_server this, http_request request)
{
    size_t i;
    const char *uri = request_get_uri(request);

    assert(this != NULL);
    assert(request != NULL);
    assert(uri != NULL);

    for (i = 0; i < this->npages; i++) {
        dynamic_page p = this->pages[i];
        if (0 == strcmp(uri, dynamic_get_uri(p))) {
            return p;
        }
    }

    return NULL;
}

void http_server_set_timeout_read(http_server this, int n)
{
    assert(this != NULL);

    this->timeout_read = n;
}

void http_server_set_timeout_write(http_server this, int n)
{
    assert(this != NULL);

    this->timeout_write = n;
}

void http_server_set_timeout_accept(http_server this, int n)
{
    assert(this != NULL);

    this->timeout_accept = n;
}

int http_server_get_timeout_read(http_server this)
{
    assert(this != NULL);

    return this->timeout_read;
}

void http_server_set_retries_read(http_server this, int seconds)
{
    assert(this != NULL);

    this->retries_read = seconds;
}

void http_server_set_retries_write(http_server this, int seconds)
{
    assert(this != NULL);

    this->retries_write = seconds;
}

int http_server_get_timeout_accept(http_server this)
{
    assert(this != NULL);

    return this->timeout_accept;
}

int http_server_get_timeout_write(http_server this)
{
    assert(this != NULL);

    return this->timeout_write;
}

void http_server_set_max_pages(http_server this, size_t n)
{
    assert(this != NULL);

    this->max_pages = n;
}

size_t http_server_get_max_pages(http_server this)
{
    assert(this != NULL);

    return this->max_pages;
}

void http_server_set_port(http_server this, int n)
{
    assert(this != NULL);

    this->port = n;
}

int http_server_get_port(http_server this)
{
    assert(this != NULL);

    return this->port;
}

/* These control the thread pool */
void http_server_set_worker_threads(http_server this, size_t n)
{
    assert(this != NULL);

    this->worker_threads = n;
}

size_t http_server_get_worker_threads(http_server this)
{
    assert(this != NULL);

    return this->worker_threads;
}

void http_server_set_queue_size(http_server this, size_t n)
{
    assert(this != NULL);

    this->queue_size = n;
}

size_t http_server_get_queue_size(http_server this)
{
    assert(this != NULL);

    return this->queue_size;
}

/* To wait when the queue is full or not */
void http_server_set_block_when_full(http_server this, int n)
{
    assert(this != NULL);

    this->block_when_full = n;
}

int http_server_get_block_when_full(http_server this)
{
    assert(this != NULL);

    return this->block_when_full;
}


int http_server_shutting_down(http_server this)
{
    assert(this != NULL);

    return this->shutting_down;
}


http_request http_server_get_request(http_server this)
{
    http_request r;

    assert(this != NULL);

    assert(this != NULL);
    assert(this->requests != NULL);

    if (!pool_get(this->requests, (void **)&r))
        r = NULL;

    assert(r != NULL);
    return r;
}

http_response http_server_get_response(http_server this)
{
    http_response r;

    assert(this != NULL);
    assert(this->requests != NULL);

    if (!pool_get(this->responses, (void **)&r))
        r = NULL;

    assert(r != NULL);
    return r;
}

void http_server_recycle_request(http_server this, http_request request)
{
    assert(this != NULL);
    assert(request != NULL);

    request_recycle(request);
    pool_recycle(this->requests, request);
}

void http_server_recycle_response(http_server this, http_response response)
{
    assert(this != NULL);
    assert(response != NULL);

    response_recycle(response);
    pool_recycle(this->responses, response);
}

status_t http_server_set_default_page_attributes(http_server this, page_attribute a)
{
    assert(this != NULL);
    assert(a != NULL);

    /* Free old attributes, if any */
    if (NULL != this->default_attributes)
        attribute_free(this->default_attributes);

    /* Copy new ones */
    this->default_attributes = attribute_dup(a);
    if (NULL == this->default_attributes)
        return failure;

    return success;
}

page_attribute http_server_get_default_attributes(http_server this)
{
    assert(this != NULL);

    return this->default_attributes;
}

status_t http_server_set_host(http_server this, const char *host)
{
    size_t n;

    assert(this != NULL);
    assert(host != NULL);

    /* Free old version, if any */
    free(this->host);

    n = strlen(host) + 1;
    this->host = malloc(n);
    if (NULL == this->host)
        return failure;

    memcpy(this->host, host, n);
    return success;
}

status_t http_server_set_logfile(http_server this, const char *name)
{
    size_t n;

    assert(this != NULL);
    assert(name != NULL);
    assert(this->logfile == NULL); /* Do not call twice */

    n = strlen(name) + 1;
    if (n > sizeof this->logfile_name)
        return fail(ENOSPC);

    memcpy(this->logfile_name, name, n);
    this->logging = 1;
    return success;
}

/* Just a small helper, assumes that the logfile_lock is locked so
 * that we don't get MT conflicts.
 */
static status_t rotate_if_needed(http_server this)
{
    char newname[LOGFILE_MAX + 102];
    char datebuf[100];
    time_t now;
    struct tm tm_now;

    assert(this != NULL);
    assert(this->logfile != NULL);

    /* Is rotating disabled? */
    if (this->logrotate == 0)
        return success;

    /* Do we need to rotate? */
    if (this->logfile_entries < this->logrotate)
        return success;

    if ((now = time(NULL)) == (time_t)-1
    || localtime_r(&now, &tm_now) == NULL
    || !strftime(datebuf, sizeof datebuf, ".%Y%m%d%H%M%S", &tm_now)) {
        warning("Could not get time");
        return failure;
    }

    strcpy(newname, this->logfile_name);
    strcat(newname, datebuf);

    if (fclose(this->logfile)) {
        warning("Could not close logfile");
        return failure;
    }

    /* Now we move away the old file by renaming it and
     * then we reopen the logfile.
     */
    if (rename(this->logfile_name, newname)) {
        char err[1024];

        if (strerror_r(errno, err, sizeof err))
            strcpy(err, "Unknown error");

        warning("Could not rename logfile, error:%s\n", err);
        return failure;
    }

    /* Open the logfile. */
    if ((this->logfile = fopen(this->logfile_name, "a")) == NULL) {
        warning("Could not open logfile %s", this->logfile_name);
        return failure;
    }

    this->logfile_entries = 0;
    return success;
}


void http_server_add_logentry(
    http_server this,
    connection conn,
    http_request request,
    int status_code,
    size_t bytes_sent)
{
    /* The common logfile format is:
     * (IP|DNS) rfc931 username [date] "request" status bytes
     * rfc and username becomes - -
     * No one uses DNS so we go for IP.
     * Date is: dayofmonth/nameofmonth/YYYY:HH:MM:SS offset_from_gmt
     * which equals "%d/%b%Y:%H:%M:%S %z";
     * I guess that the %z doesn't port very well :-(
     */
    static const char *format = "%d/%b/%Y:%H:%M:%S %z";
    const char *method;
    time_t now;
    struct tm tm_now;
    char datebuf[100];
    int cc;

    /* This string used to be INET_ADDRSTRLEN + 1 chars long, but that constant
     * plus inet_ntop() ports really bad to other systems (HP-UX/cygWindows).
     * Haven't found a reentrant version of inet_ntoa yet, so I'll leave it
     * as is for now.
     */
    char ip[200];

    assert(this != NULL);
    assert(status_code != 0);

    /* Has logging been enabled? */
    if (!this->logging)
        return;

    if (pthread_mutex_lock(&this->logfile_lock))
        return;

    /* Open logfile if it is closed */
    if (this->logfile == NULL) {
        if ((this->logfile = fopen(this->logfile_name, "w")) == NULL) {
            /* Hmm, unable to open logfile. Disable logging */
            this->logging = 0;
            warning("Unable to open logfile %s", this->logfile_name);
            pthread_mutex_unlock(&this->logfile_lock);
            return;
        }
    }

    if (!rotate_if_needed(this)) {
        /* Hmm, unable to rotate logfile. Disable logging */
        this->logging = 0;
        warning("Unable to rotate logfile %s", this->logfile_name);
        pthread_mutex_unlock(&this->logfile_lock);
        return;
    }

    switch (request_get_method(request)) {
        case METHOD_GET:
            method = "GET";
            break;

        case METHOD_HEAD:
            method = "HEAD";
            break;

        case METHOD_POST:
            method = "POST";
            break;

        default:
            method="unknown";
            break;
    }

    if ((now = time(NULL)) == (time_t)-1
    || localtime_r(&now, &tm_now) == NULL
    || !strftime(datebuf, sizeof datebuf, format, &tm_now)) {
        warning("Could not get time");
        pthread_mutex_unlock(&this->logfile_lock);
        return;
    }

    inet_ntop(AF_INET, &connection_get_addr(conn)->sin_addr, ip, sizeof ip),

    cc = fprintf(this->logfile, "%s - - [%s] \"%s %s\" %d %lu\n",
        ip,
        datebuf,
        method,
        request_get_uri(request),
        status_code ,
        (unsigned long)bytes_sent);

    if (cc <= 0) {
        this->logging = 0;
        fclose(this->logfile);
        warning("Unable to log to logfile %s. Disabling logging", this->logfile_name);
    }
    else {
        fflush(this->logfile);
        this->logfile_entries++;
    }

    pthread_mutex_unlock(&this->logfile_lock);
}

status_t http_server_shutdown(http_server this)
{
    assert(this != NULL);
    this->shutting_down = 1;
    tcp_server_shutdown(this->tcpsrv);
    return success;
}

status_t http_server_get_root_resources(http_server this)
{
    assert(this != NULL);

    if (!configure_tcp_server(this))
        return failure;

    if (!tcp_server_get_root_resources(this->tcpsrv))
        return failure;

    return success;
}

status_t http_server_free_root_resources(http_server this)
{
    /* NOTE: 2005-11-27: Check out why we don't close the socket here. */
    (void)this;
    return success;
}

bool http_server_has_default_page_handler(http_server this)
{
    assert(this != NULL);
    return this->default_handler != NULL;
}

/* Here we create a dummy dynamic_page and use the function
 * we always use, handle_dynamic().
 * NOTE: This is slow, as we allocate and free memory for each request. :-(
 */
status_t http_server_run_default_page_handler(
    connection conn,
    http_server this,
    http_request request,
    http_response response,
    error e)
{
    dynamic_page p;
    const char *uri;
    status_t rc;

    uri = request_get_uri(request);
    if ((p = dynamic_new(uri, this->default_handler, NULL)) == NULL)
        return set_os_error(e, ENOMEM);

    rc = handle_dynamic(conn, this, p, request, response, e);
    dynamic_free(p);
    return rc;
}

status_t http_server_start_via_process(process p, http_server this)
{
    return process_add_object_to_start(
        p,
        this,
        (status_t(*)(void*))http_server_get_root_resources,
        (status_t(*)(void*))http_server_free_root_resources,
        (status_t(*)(void*))http_server_start,
        (status_t(*)(void*))http_server_shutdown);
}

status_t http_server_configure(http_server this, process p, const char *filename)
{
    int port = -1;
    int workers = -1;
    int queuesize = -1;
    int block_when_full = -1;
    int timeout_read = -1;
    int timeout_write = -1;
    int timeout_accept = -1;
    int retries_read = -1;
    int retries_write = -1;
    int logrotate = -1;

    char hostname[1024] = { '\0' };
    char logfile[1024] = { '\0' };
    char username[1024] = { '\0' };
    char rootdir[1024] = { '\0' };
    char docroot[10240] = { '\0' };

    configfile cf;

    assert(this != NULL);
    assert(filename != NULL);

    if ((cf = configfile_read(filename)) == NULL)
        return failure;

    if (configfile_exists(cf, "workers")
    && !configfile_get_int(cf, "workers", &workers))
        goto readerr;

    if (configfile_exists(cf, "queuesize")
    && !configfile_get_int(cf, "queuesize", &queuesize))
        goto readerr;

    if (configfile_exists(cf, "block_when_full")
    && !configfile_get_int(cf, "block_when_full", &block_when_full))
        goto readerr;

    if (configfile_exists(cf, "timeout_read")
    && !configfile_get_int(cf, "timeout_read", &timeout_read))
        goto readerr;

    if (configfile_exists(cf, "timeout_write")
    && !configfile_get_int(cf, "timeout_write", &timeout_write))
        goto readerr;

    if (configfile_exists(cf, "retries_read")
    && !configfile_get_int(cf, "retries_read", &retries_read))
        goto readerr;

    if (configfile_exists(cf, "retries_write")
    && !configfile_get_int(cf, "retries_write", &retries_write))
        goto readerr;

    if (configfile_exists(cf, "logrotate")
    && !configfile_get_int(cf, "logrotate", &logrotate))
        goto readerr;

    if (configfile_exists(cf, "username")
    && !configfile_get_string(cf, "username", username, sizeof username))
        goto readerr;

    if (configfile_exists(cf, "rootdir")
    && !configfile_get_string(cf, "rootdir", rootdir, sizeof rootdir))
        goto readerr;

    if (configfile_exists(cf, "documentroot")
    && !configfile_get_string(cf, "documentroot", docroot, sizeof docroot))
        goto readerr;

    if (configfile_exists(cf, "port") && !configfile_get_int(cf, "port", &port))
        goto readerr;

    if (configfile_exists(cf, "hostname")
    && !configfile_get_string(cf, "hostname", hostname, sizeof hostname))
        goto readerr;

    if (configfile_exists(cf, "logfile")
    && !configfile_get_string(cf, "logfile", logfile, sizeof logfile))
        goto readerr;

    configfile_free(cf);

    if (port != -1)
        http_server_set_port(this, port);

    if (retries_read != -1)
        http_server_set_retries_read(this, retries_read);

    if (retries_write != -1)
        http_server_set_retries_write(this, retries_write);

    if (logrotate != -1)
        http_server_set_logrotate(this, logrotate);

    if (timeout_read != -1)
        http_server_set_timeout_read(this, timeout_read);

    if (timeout_write != -1)
        http_server_set_timeout_write(this, timeout_write);

    if (timeout_accept != -1)
        http_server_set_timeout_accept(this, timeout_accept);

    if (block_when_full != -1)
        http_server_set_block_when_full(this, block_when_full);

    if (queuesize != -1)
        http_server_set_queue_size(this, (size_t)queuesize);

    if (workers != -1)
        http_server_set_worker_threads(this, (size_t)workers);

    if (strlen(hostname) > 0 && !http_server_set_host(this, hostname))
        return failure;

    if (strlen(logfile) > 0 && !http_server_set_logfile(this, logfile))
        return failure;

    if (strlen(docroot) > 0 && !http_server_set_documentroot(this, docroot))
        return failure;

    /* Now for process stuff */
    if (p == NULL)
        return success;

    if (strlen(username) > 0
    && getuid() == 0
    && !process_set_username(p, username))
        return failure;

    if (strlen(rootdir) > 0
    && getuid() == 0
    && !process_set_rootdir(p, rootdir))
        return failure;

    return success;

readerr:
    configfile_free(cf);
    return failure;
}

unsigned long http_server_sum_blocked(http_server this)
{
    assert(this != NULL);
    return tcp_server_sum_blocked(this->tcpsrv);
}

unsigned long http_server_sum_discarded(http_server this)
{
    assert(this != NULL);
    return tcp_server_sum_discarded(this->tcpsrv);
}

unsigned long http_server_sum_added(http_server this)
{
    assert(this != NULL);
    return tcp_server_sum_added(this->tcpsrv);
}

unsigned long http_server_sum_poll_intr(http_server p)
{
    assert(p != NULL);
    return tcp_server_sum_poll_intr(p->tcpsrv);
}

unsigned long http_server_sum_poll_again(http_server p)
{
    assert(p != NULL);
    return tcp_server_sum_poll_again(p->tcpsrv);
}

unsigned long http_server_sum_accept_failed(http_server p)
{
    assert(p != NULL);
    return tcp_server_sum_accept_failed(p->tcpsrv);
}

unsigned long http_server_sum_denied_clients(http_server p)
{
    assert(p != NULL);
    return tcp_server_sum_denied_clients(p->tcpsrv);
}

status_t http_server_set_rootcert(http_server this, const char *path)
{
    assert(this != NULL);
    assert(this->tcpsrv != NULL);

    return tcp_server_set_rootcert(this->tcpsrv, path);
}

status_t http_server_set_private_key(http_server this, const char *path)
{
    assert(this != NULL);
    assert(this->tcpsrv != NULL);

    return tcp_server_set_private_key(this->tcpsrv, path);
}

status_t http_server_set_ciphers(http_server this, const char *ciphers)
{
    assert(this != NULL);
    assert(this->tcpsrv != NULL);

    return tcp_server_set_ciphers(this->tcpsrv, ciphers);
}

status_t http_server_set_ca_directory(http_server this, const char *path)
{
    assert(this != NULL);
    assert(this->tcpsrv != NULL);

    return tcp_server_set_ca_directory(this->tcpsrv, path);
}

#ifdef CHECK_HTTP_SERVER

static const char* html = "<html><head><title>hello</title></head><body>world</body></html>";
static int pageserver(http_request req, http_response response)
{
    (void)req;
    if (!response_add(response, html))
        die("Could not add html code to response.\n");

    return 200;
}


/* A common problem is that we screw up the read/buffering logic
 * in the server. Therefore, we add a test which asserts that
 * the response time never exceeds some sane limit. */
static void * serverthread(void *arg)
{
    http_server this = arg;

    if (!http_server_alloc(this))
        die("Could not allocate resources.\n");

    http_server_set_port(this, 2000);

    http_server_set_default_page_handler(this, pageserver);

    if (!http_server_get_root_resources(this))
        die("%s:Could not get root resources\n", __func__);

    if (!http_server_start(this))
        die("Could not start server:%s\n", strerror(errno));

    sleep(this->timeout_accept / 1000 + 1);

    http_server_free(this);
}

static void make_request(void)
{
    http_client p;
    http_response resp;

    const char *hostname ="localhost";
    const char *uri = "/";
    int port = 2000;

    if ((p = http_client_new()) == NULL)
        exit(1);

    if (!http_client_connect(p, hostname, port)) {
        fprintf(stderr, "Could not connect to %s\n", hostname);
        exit(1);
    }

    if (!http_client_get(p, hostname, uri)) {
        fprintf(stderr, "Could not get %s from %s\n", uri, hostname);
        http_client_disconnect(p);
        exit(1);
    }

    if (!http_client_disconnect(p)) {
        fprintf(stderr, "Could not disconnect from %s\n", hostname);
        exit(1);
    }

    resp = http_client_response(p);
    // Did we get what we expected?
    if (strcmp(html, response_get_entity(resp)) != 0) {
        fprintf(stderr, "Expected \"%s\", got \"%s\"\n", html, response_get_entity(resp));
    }

    http_client_free(p);
}

static void check_response_time(void)
{
    http_server this = http_server_new(SOCKTYPE_TCP);
    pthread_t t;
    int err;
    clock_t start, stop;
    double duration, max_duration = 0.05;

    if ((err = pthread_create(&t, NULL, serverthread, this)))
        die("Could not start server thread\n");

    // Give server time to bind..
    sleep(1);

    // Now make a request
    start = clock();
    make_request();
    stop = clock();
    duration = stop - start;
    duration /= CLOCKS_PER_SEC;
    if (duration > max_duration) {
        fprintf(stderr, "Server too slow, spent %g seconds,"
            " which is above threshold of %g secs.\n", duration, max_duration);
        exit(1);
    }

    sleep(1);
    if (!http_server_shutdown(this))
        die("Could not shutdown server.\n");


    if (pthread_join(t, NULL))
        die("Could not join server thread.\n");
}

int main(void)
{
    check_response_time();
    return 0;
}
#endif
