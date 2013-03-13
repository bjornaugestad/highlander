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

