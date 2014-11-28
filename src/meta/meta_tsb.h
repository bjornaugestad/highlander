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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef META_TSB_H
#define META_TSB_H

/* @file meta_tsb.h
 * tsb is short for Time Shared Buffer. It is an ADT which lets threads
 * time share a common buffer. The idea is to use the ADT to simulate
 * physical elements like a radio transmission.
 *
 * One can have 1..n writer threads, the senders, and 0..n reader threads,
 * the receivers. The most common configuration is probably to have
 * either one sender and many receivers or many senders and one receiver.
 * This reflects the communication between one base station (INA) and
 * one or more set top boxes (NIU's).
 *
 * It tsb works like this: First decide the time duration you want to
 * use for the time units, in milliseconds(ms). Then decide how long a time
 * frame is, in time units. Now you can split the time frame into several
 * subframes and assign threads to each subframe. The tsb will create the
 * threads for you and assert that the threads only execute within the
 * assigned time units for the subframe.
 *
 * An example: Let's say that the hardware has a clock with a resolution of
 * 10 milliseconds. To be on the safe side, we use 20 ms as duration for
 * each time unit. We know that other threads in the program will require
 * some CPU, so we decide that a time frame will be 4 time units long.
 * Now, let's assign time unit 0 to one writer thread and time unit
 * 3 to 4 reader threads. We leave time unit 2 and 4 unused, just like
 * a guard interval, in case the callback function is a bit slow or the
 * scheduler is slow.
 * Start the tsb whenever the rest of the system is set up.
 *
 * @note Compile with _BSD_SOURCE defined to get the timeradd()
 * and timersub() macros used to manipulate struct timeval.
 *
 * @todo Consider limiting the number of thread sets to 2 and require
 * that one of the sets have 1..n threads and the other has 1..1 thread.
 * The upside is that we then can do error checking using a rw lock with
 * pthread_rwlock_tryrdlock() and trywrlock(). That is a very good thing
 * in case the callback functions are too slow and doesn't finish within
 * time. The downside is that we then cannot have multiple sets of n..n
 * threads. Is that a bad thing???
 */

#include <sys/time.h>	/* for struct timeval and tsb_get_epoch() */

#ifdef __cplusplus
extern "C" {
#endif

/* Declaration of our tsb ADT */
typedef struct tsb_tag tsb;

/*
 * Creates a new tsb.
 */
tsb* tsb_new(size_t unit_duration, size_t units_per_frame, void* buffer);

/*
 * Deletes a tsb and frees all memory allocated by the tsb. The shared buffer
 * will not be freed.
 */
void tsb_free(tsb* p);

/*
 * Returns the duration of each unit
 */
size_t tsb_get_unit_duration(tsb* p);

/*
 * Returns the time that the tsb was started. You can only call this function
 * after tsb_start() has been called. Use it to synchronize multiple tsb's.
 * More specificly: You need the epoch of the dvbrct tsb to be able to
 * compute the start time of the upcoming transmission frames.
 * @note Maybe we should add a tsb_get_next_frame_start() function? Nice to
 * have... ;-)
 */
const struct timeval* tsb_get_epoch(tsb* p);

/*
 * get number of time units per frame
 */
size_t tsb_get_units_per_frame(tsb* p);

/*
 *
 */
size_t tsb_get_current_unit(tsb* p);

/*
 *
 */
size_t tsb_get_current_frame(tsb* p);

/*
 * Returns a pointer to the shared buffer assigned to the tsb in tsb_new().
 */
void* tsb_get_buffer(tsb* p);

/*
 * Assign a callback function and number of threads to one or more
 * time units. The callback function is called once per time frame
 * and gets both the buffer and the args assigned to the tsb as arguments.
 *
 * The callback function must return 1 if it wants the thread to continue
 * running, or 0 if it wants to stop running. So if a NIU wants to disconnect,
 * just return 0.
 */
int tsb_set_threads(
	tsb *p,
	size_t iunit, /* Run thread(s) when iunit within frame starts */
	size_t nthreads,
	int (*callback)(void* buf, void* arg),
	void* arg);


/*
 * Start the threads and sets the epoch of the tsb.
 */
int tsb_start(tsb* p);

/*
 * Stops all threads.
 */
int tsb_stop(tsb* p);

#ifdef __cplusplus
}
#endif

#endif
