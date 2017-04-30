#include <stdio.h>
#include <errno.h>

#include <meta_atomic.h>
#include <meta_sampler.h>

#include <httpcache.h>

/*
 * This is our statistics package, maintaining all the stat info we later want 
 * to report back to the user. 
 *
 * So what do we store here and how is the info updated and read? 
 * First of all the handle_requests() function increments a set of counters:
 * - Currently running instances of handle_requests()
 * - The return status of each request(200/304/500/404 and more)
 * - Number of successful requests
 * - Number of failed requests
 * - Bytes transfered to user 
 * - And More...
 * We use fast fully inlined variables and functions for this and place these
 * variables in handle_requests(), so that updates are as fast as possible. 
 * Here we just read them once a minute so an extern should be OK.
 */
extern atomic_u32 sum_requests;
extern atomic_ull sum_bytes;
extern atomic_u32 sum_200;
extern atomic_u32 sum_304;
extern atomic_u32 sum_400;
extern atomic_u32 sum_404;
extern atomic_u32 sum_500;

extern http_server g_server;

/*
 *
 * All these items are single instance variables written to by 
 * handle_requests().
 *
 * We also want to log changes over time. To do that, we start a new thread
 * that every minute(or second?) reads the variables and updates a 
 * meta_sampler object. We have other meta_sampler objects too, 
 * one with 24 hours, one with 7 days and one with 52 weeks.
 * That way we can store history for the last year with reasonable resolution.
 */


static int m_shutting_down = 0;
static sampler m_sampler;
static pthread_t m_thread_id;

/* What we sample */
#define SE_REQUESTS 0
#define SE_BYTES    1
#define SE_200      2
#define SE_304      3
#define SE_400      4
#define SE_404      5
#define SE_500      6

#define SE_BLOCKED  7 /* Number of connections blocked in the threadpool */
#define SE_DISCARDED  8 /* Number of connections discarded from the threadpool */
#define SE_ADDED      9 /* Number of connections added to the threadpool */
#define SE_POLL_INTR 10
#define SE_POLL_AGAIN 11
#define SE_ACCEPT_FAILED 12
#define SE_DENIED_CLIENTS 13
#define SUM_ENTITIES 14

static int sample_data(sampler s)
{
    /* We only store the delta so this is the data for the 
     * previous(last) sampling */
    static uint32_t last_requests;
    static unsigned long long last_bytes;
    static uint32_t last_200;
    static uint32_t last_304;
    static uint32_t last_400;
    static uint32_t last_404;
    static uint32_t last_500;
    static unsigned long last_blocked;
    static unsigned long last_discarded;
    static unsigned long last_added;
    static unsigned long last_poll_intr;
    static unsigned long last_poll_again;
    static unsigned long last_accept_failed;
    static unsigned long last_denied_clients;

    /* Gather all the data we want and stuff it into the sampler object. 
     * Remember that the entities must be indexed from 0..n.
     */
    uint32_t requests = atomic_u32_get(&sum_requests);
    unsigned long long bytes = atomic_ull_get(&sum_bytes);
    uint32_t _200 = atomic_u32_get(&sum_200);
    uint32_t _304 = atomic_u32_get(&sum_304);
    uint32_t _400 = atomic_u32_get(&sum_400);
    uint32_t _404 = atomic_u32_get(&sum_404);
    uint32_t _500 = atomic_u32_get(&sum_500);

    unsigned long blocked = http_server_sum_blocked(g_server);
    unsigned long discarded = http_server_sum_discarded(g_server);
    unsigned long added = http_server_sum_added(g_server);
    unsigned long poll_intr = http_server_sum_poll_intr(g_server);
    unsigned long poll_again = http_server_sum_poll_again(g_server);
    unsigned long accept_failed = http_server_sum_accept_failed(g_server);
    unsigned long denied_clients = http_server_sum_denied_clients(g_server);

    time_t now = time(NULL);
    sampler_start_update(s, now);
    sampler_add(s, SE_REQUESTS, requests - last_requests);
    sampler_add(s, SE_BYTES, bytes - last_bytes);
    sampler_add(s, SE_200, _200 - last_200);
    sampler_add(s, SE_304, _304 - last_304);
    sampler_add(s, SE_400, _400 - last_400);
    sampler_add(s, SE_404, _404 - last_404);
    sampler_add(s, SE_500, _500 - last_500);
    sampler_add(s, SE_BLOCKED, blocked - last_blocked);
    sampler_add(s, SE_DISCARDED, discarded - last_discarded);
    sampler_add(s, SE_ADDED, added - last_added);
    sampler_add(s, SE_POLL_INTR, poll_intr - last_poll_intr);
    sampler_add(s, SE_POLL_AGAIN, poll_again - last_poll_again);
    sampler_add(s, SE_ACCEPT_FAILED, accept_failed - last_accept_failed);
    sampler_add(s, SE_DENIED_CLIENTS, denied_clients - last_denied_clients);
    sampler_commit(s);

    last_requests = requests;
    last_bytes = bytes;
    last_200 = _200;
    last_304 = _304;
    last_400 = _400;
    last_404 = _404;
    last_500 = _500;
    last_blocked = blocked;
    last_discarded = discarded;
    last_added = added;
    last_poll_intr = poll_intr;
    last_poll_again = poll_again;
    last_accept_failed = accept_failed;
    last_denied_clients = denied_clients;
    return 1;
}


static void* sampler_thread(void* arg)
{
    sampler s = arg;
    struct timespec ts = {1, 0}, rem;


    assert(s != NULL);

    /* Wake up every minute and sample the data. 
     * To get the resolution right, we sleep for less than
     * a minute and tests to see if we have entered a new 
     * minute. For this to work, we must sleep for <= max tolerance
     * which in our case is one second. We therefore sleep for 1 sec.
     */
    time_t last = time(NULL);
    while (!m_shutting_down) {
        time_t now = time(NULL);
        if (difftime(now, last) >= 60.0) { 
            sample_data(s);
            last = now;
        }

        if (nanosleep(&ts, &rem) == -1 && errno == EINTR) {
            /* Just try to sleep the remaining period, ignore failure */
            nanosleep(&rem, NULL); 
        }
    }

    return NULL;
}

int statpack_start(void)
{
    m_shutting_down = 0;
    if ( (m_sampler = sampler_new(SUM_ENTITIES, 24*60)) == NULL) {
        return 0;
    }
    else if (pthread_create(&m_thread_id, NULL, sampler_thread, m_sampler)) {
        sampler_free(m_sampler);
        return 0;
    }
    else {
        return 1;
    }
}

int statpack_stop(void)
{
    m_shutting_down = 1;
    pthread_join(m_thread_id, NULL);
    sampler_free(m_sampler);
    return 1;
}


/* Our callback function */
/* So what do we want to show here, and how do we want it presented to the user?
 * Let's start off with a simple table containing the sum info we have.
 */
int show_stats(http_request req, http_response page)
{
    char buf[1024];
    const char* wip = "Work in progress";

    /* We copy the sampler to avoid blocking operations */
    sampler dup = sampler_dup(m_sampler);

    uint32_t requests = atomic_u32_get(&sum_requests);
    unsigned long long bytes = atomic_ull_get(&sum_bytes);
    uint32_t _200 = atomic_u32_get(&sum_200);
    uint32_t _304 = atomic_u32_get(&sum_304);
    uint32_t _400 = atomic_u32_get(&sum_400);
    uint32_t _404 = atomic_u32_get(&sum_404);
    uint32_t _500 = atomic_u32_get(&sum_500);
    unsigned long blocked = http_server_sum_blocked(g_server);
    unsigned long discarded = http_server_sum_discarded(g_server);
    unsigned long added = http_server_sum_added(g_server);
    unsigned long poll_intr = http_server_sum_poll_intr(g_server);
    unsigned long poll_again = http_server_sum_poll_again(g_server);
    unsigned long accept_failed = http_server_sum_accept_failed(g_server);
    unsigned long denied_clients = http_server_sum_denied_clients(g_server);

    (void)req;
    sprintf(buf, "%lu", (unsigned long)requests);

    if (!add_page_start(page, PAGE_STATS)
    || !response_add(page, "<table columns=3>\n<th>Category</th>\n<th>Sum</th><th>Last minute</th>\n")
    || !response_add(page, "<tr><td><b>webcache counters</b></td></tr>\n")
    || !response_add(page, "<tr><td>Requests served</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%llu", bytes);
    if (!response_add(page, "<tr><td>Bytes sent</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)_200);
    if (!response_add(page, "<tr><td>status code 200</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)_304);
    if (!response_add(page, "<tr><td>status code 304</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)_400);
    if (!response_add(page, "<tr><td>status code 400</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;


    sprintf(buf, "%lu", (unsigned long)_404);
    if (!response_add(page, "<tr><td>status code 404</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)_500);
    if (!response_add(page, "<tr><td>status code 500</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)added);
    if (!response_add(page, "<tr><td><b>TCP server counters</b></td></tr>\n")
    || !response_add(page, "<tr><td>Connection requests accepted</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)blocked);
    if (!response_add(page, "<tr><td>Connection requests blocked</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)discarded);
    if (!response_add(page, "<tr><td>Connection requests discarded</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)poll_intr);
    if (!response_add(page, "<tr><td>poll() was interrupted</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)poll_again);
    if (!response_add(page, "<tr><td>poll() returned EAGAIN</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)accept_failed);
    if (!response_add(page, "<tr><td>accept() returned -1</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    sprintf(buf, "%lu", (unsigned long)denied_clients);
    if (!response_add(page, "<tr><td>Denied clients</td>\n")
    || !response_td(page, buf)
    || !response_td(page, wip))
        goto err;

    if (!response_add(page, "</table>\n")
    || !add_page_end(page, NULL))
        goto err;

    sampler_free(dup);
    return 0;

err:
    sampler_free(dup);
    return HTTP_500_INTERNAL_SERVER_ERROR;
}
