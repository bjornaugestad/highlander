/*
    libcoremeta - A library of useful C functions and ADT's
    Copyright (C) 2000-2006 B. Augestad, bjorn.augestad@gmail.com 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef META_TICKER_H
#define META_TICKER_H

/**
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

#endif /* guard */
 
