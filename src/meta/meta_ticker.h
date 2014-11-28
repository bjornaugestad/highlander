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

#ifndef META_TICKER_H
#define META_TICKER_H

/*
 * A ticker is a thread which at certain intervals performs
 * some caller defined actions.
 */
typedef struct ticker_tag* ticker;

/* Create a ticker which wakes up every usec microseconds
 * and then performs the actions it knows about.
 */
ticker ticker_new(int usec);
void   ticker_free(ticker t);

/* Add an action to be executed every tick */
int ticker_add_action(ticker t, void(*pfn)(void*), void* arg);

/* Start the ticker. */
int ticker_start(ticker t);
void ticker_stop(ticker t);

#endif
