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
#ifndef META_WLOCK_H
#define META_WLOCK_H

/**
 * The meta_wlock adt implements a waitable/signalable lock
 * (Just a mutex/condvar combo) as one atomic type.
 *
 * We also want to have multiple references to the same pair of mutex/locks
 * so we have a refcount member. Beware of a big giant lock in the 
 * implementation of this adt.
 */


#ifdef __cplusplus
extern "C" {
#endif

typedef struct wlock_tag* wlock;

wlock wlock_new(void);		/* Create a new wlock */
void  wlock_free(wlock p);	/* destroy a wlock (if refcount == 1) */
int   wlock_lock(wlock p);	/* Lock it */
int   wlock_unlock(wlock p);/* Unlock it */
int   wlock_signal(wlock p);/* Signal it */
int   wlock_wait(wlock p);	/* Wait for the wlock to be signalled */
int   wlock_broadcast(wlock p);

#ifdef __cplusplus
}
#endif

#endif /* guard */

