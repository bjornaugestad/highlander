/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>

#include <meta_common.h>
#include <meta_tsb.h>

/**
 * We store 1 of these per time unit.
 */
struct handler {
    size_t nthreads;

    /* Callback function pointer */
    int (*callback)(void* buffer, void* arg);

    /* Extra argument to callback */
    void* arg;

    /* Our own pthread_t, one for each thread within time unit */
    pthread_t* threads;
};

/**
 * The Time Shared Buffer ADT
 */
struct tsb_tag {
    /* duration of one time unit, in milliseconds */
    size_t unit_duration;

    /* Units per frame */
    size_t units_per_frame;

    /* Generic pointer to the time shared buffer */
    void* buffer;

    struct timeval epoch;

    /* Pointer to the handlers, one for each time unit in the frame.
     * If the nthreads member is 0, then the entry is invalid.
     */
    struct handler* handlers;

    /*
     * The threads test this flag for shutdown notification.
     */
    int shutdown_flag;
};


tsb* tsb_new(size_t unit_duration, size_t units_per_frame, void* buffer)
{
    tsb* p;

    assert(unit_duration != 0);
    assert(units_per_frame != 0);

    if ((p = malloc(sizeof *p)) == NULL
    ||  (p->handlers = malloc(sizeof *p->handlers * units_per_frame)) == NULL) {
        free(p);
        p = NULL;
    }
    else {
        p->unit_duration = unit_duration;
        p->units_per_frame = units_per_frame;
        p->buffer = buffer;
        p->shutdown_flag = 0;
        memset(p->handlers, '\0', sizeof *p->handlers * units_per_frame);
    }

    return p;
}

void tsb_free(tsb* p)
{
    size_t i;
    assert(p != NULL);
    assert(p->shutdown_flag); /* Don't free before shutdown */

    for (i = 0; i < p->units_per_frame; i++) {
        if (p->handlers[i].threads != NULL)
            free(p->handlers[i].threads);
    }

    free(p->handlers);
    free(p);
}

const struct timeval* tsb_get_epoch(tsb* p)
{
    assert(p != NULL);
    return &p->epoch;
}

size_t tsb_get_unit_duration(tsb* p)
{
    assert(p != NULL);
    return p->unit_duration;
}

size_t tsb_get_units_per_frame(tsb* p)
{
    assert(p != NULL);
    return p->units_per_frame;
}

void* tsb_get_buffer(tsb* p)
{
    assert(p != NULL);
    return p->buffer;
}

int tsb_set_threads(
    tsb *p,
    size_t iunit,
    size_t nthreads,
    int (*callback)(void* buf, void* arg),
    void* arg)
{
    assert(p != NULL);
    assert(p->handlers != NULL);
    assert(iunit < p->units_per_frame);
    assert(nthreads > 0);
    assert(callback != NULL);

    p->handlers[iunit].nthreads = nthreads;
    p->handlers[iunit].callback = callback;
    p->handlers[iunit].arg = arg;
    if ((p->handlers[iunit].threads = calloc(nthreads, sizeof *p->handlers[iunit].threads)) == NULL)
        return 0;

    return 1;
}

/**
 * Returns the number of units since our epoch.
 * Recall that a unit == time unit, which is specified in milliseconds
 */
static size_t units_since_epoch(
    struct timeval* epoch,
    struct timeval* now,
    size_t unit_duration)
{
    struct timeval diff;
    size_t ms, units;

    /* Compute difference in time */
    timersub(now, epoch, &diff);

    /* Now we have time elapsed since (our) epoch.
     * How many units have passed?
     */
    units = diff.tv_sec * (1000 / unit_duration);

    /* Now for the microseconds */
    ms = (diff.tv_usec / 1000);

    units += (ms / unit_duration);
    return units;
}

/**
 * Create a timeval struct, dest, containing a proper time. The time
 * is offset since epoch, measured in units. Each unit has a duration
 * of unit_duration ms, so beware of overflows.
 */
static void unit_to_timeval(
    struct timeval* epoch,
    struct timeval* dest,
    size_t unit,
    size_t unit_duration)
{
    struct timeval tv;
    size_t rest, seconds, units_per_second;

    units_per_second = 1000/ unit_duration;
    assert(units_per_second != 0);

    seconds = (unit / units_per_second);
    rest = (unit % units_per_second);

    tv.tv_sec = seconds;

    /*
     * Resolution is in ms, but usec is in us. We multiply with 1000
     * to get things right.
     */
    tv.tv_usec = (rest * unit_duration * 1000);
    timeradd(epoch, &tv, dest);
}

/**
 * Compute the difference between now and when, then convert
 * things to a timespec struct and nanosleep() for a while.
 */
static void timeval_sleep(
    struct timeval* now,
    struct timeval* when)
{
    struct timeval diff;
    struct timespec ts;

    timersub(when, now, &diff);

    ts.tv_sec = diff.tv_sec;
    ts.tv_nsec = diff.tv_usec * 1000;

    nanosleep(&ts, NULL);
}

size_t tsb_get_current_unit(tsb* p)
{
    struct timeval now;

    assert(p != NULL);

    gettimeofday(&now, NULL);
    return units_since_epoch(&p->epoch, &now, p->unit_duration);
}

size_t tsb_get_current_frame(tsb* p)
{
    assert(p != NULL);
    return tsb_get_current_unit(p) / p->units_per_frame;
}


/**
 * The thread function we use. It requires a couple of arguments,
 * the tsb itself as well as the unit index to run in. Since thread functions
 * accept only one argument, we need a separate struct.
 */
struct args {
    size_t iunit;
    tsb* ptsb;
};

static void* threadfunc(void* arg)
{
    struct args* parg = arg;
    struct timeval now, next;
    size_t units, offset;
    int rc;

    /* Loop until shutdown */
    while (!parg->ptsb->shutdown_flag) {

        /* What time is it right now? */
        gettimeofday(&now, NULL);

        /* How many time units have passed? */
        units = units_since_epoch(
            &parg->ptsb->epoch,
            &now,
            parg->ptsb->unit_duration);

        /* Now we need to know if we're supposed to wait for
         * a unit within the current frame or for a unit within
         * the next frame. I guess we always wait for the next frame
         * since we supposedly already have waited for the unit
         * in the current frame. This means that we add x units plus
         * our own offset. x is the number of units which are left in
         * the current frame. Easier than we may think, since all we
         * have to do is mod the number of units.
         */
        offset = units % parg->ptsb->units_per_frame;
        units += parg->ptsb->units_per_frame - offset;

        /* Now add our own offset */
        units += parg->iunit;

        /* Convert the index to a proper timeval */
        unit_to_timeval(
            &parg->ptsb->epoch,
            &next,
            units,
            parg->ptsb->unit_duration);

        /* Now sleep until we reach the proper slot */
        timeval_sleep(&now, &next);

        /* And finally we're ready to call the callback function */
        rc = parg->ptsb->handlers[parg->iunit].callback(
            parg->ptsb->buffer,
            parg->ptsb->handlers[parg->iunit].arg);

        /* Exit if error in callback function */
        if (rc == 0)
            break;
    }

    free(arg);
    return NULL;
}

int tsb_start(tsb* p)
{
    size_t i, j;
    int err;

    assert(p != NULL);
    assert(p->handlers != NULL);

    /* Set our epoch */
    gettimeofday(&p->epoch, NULL);

    /* Start all threads */
    for (i = 0; i < p->units_per_frame; i++) {
        for (j = 0; j < p->handlers[i].nthreads; j++) {
            struct args* pargs;
            if ((pargs = malloc(sizeof *pargs)) == NULL)
                return 0;

            pargs->iunit = i;
            pargs->ptsb = p;
            err = pthread_create(&p->handlers[i].threads[j], NULL, threadfunc, pargs);
            if (err)
                return 0;
        }
    }

    return 1;
}

int tsb_stop(tsb* p)
{
    size_t i, j;

    assert(p != NULL);

    /* Shut down the threads */
    p->shutdown_flag = 1;

    /* Now wait for the threads to finish */
    for (i = 0; i < p->units_per_frame; i++) {
        for (j = 0; j < p->handlers[i].nthreads; j++) {
            pthread_join(p->handlers[i].threads[j], NULL);
        }
    }

    return 1;
}

#ifdef CHECK_TSB

#include <stdio.h>
struct t {
    char foo[100];
};

/* Will just enhance output by printing start of frame message: */
static int firstslot(void* buf, void* arg)
{
    (void)buf;
    (void)arg;
    fprintf(stderr, "START OF FRAME\n");
    return 1;
}

static int mywriter(void* buf, void* arg)
{
    struct timeval tv;
    double t;

    (void)buf;
    (void)arg;
    gettimeofday(&tv, NULL);
    t = tv.tv_sec + (tv.tv_usec * 1.0 / 1000000);

    fprintf(stderr, "I am in writer callback, time is: %f\n", t);
    return 1;
}

static int myreader(void* buf, void* arg)
{
    struct timeval tv;
    double t;

    (void)buf;
    (void)arg;
    gettimeofday(&tv, NULL);
    t = tv.tv_sec + (tv.tv_usec * 1.0 / 1000000);

    fprintf(stderr, "I am in reader callback, time is: %f\n", t);
    return 1;
}
#define UNIT_DURATION 50
#define UNITS_PER_FRAME 40

int main(void)
{
    char buf[1024];

    tsb* p;

    p = tsb_new(UNIT_DURATION, UNITS_PER_FRAME, buf);
    assert(p != NULL);

    tsb_set_threads(p, 0, 1, firstslot, NULL);
    tsb_set_threads(p, 2, 1, mywriter, NULL);
    tsb_set_threads(p, 4, 1, mywriter, NULL);
    tsb_set_threads(p, 6, 1, mywriter, NULL);
    tsb_set_threads(p, 7, 1, mywriter, NULL);
    tsb_set_threads(p, 8, 1, mywriter, NULL);
    tsb_set_threads(p, 20, 4, myreader, NULL);

    tsb_start(p);

    /* Let threads run for a while */
    sleep(20);

    tsb_stop(p);
    tsb_free(p);
    return 0;
}

#endif

