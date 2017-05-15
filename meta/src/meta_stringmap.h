/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_STRINGMAP_H
#define META_STRINGMAP_H

#include <stdbool.h>

#include <meta_common.h>
#include <meta_list.h>

/*
 * @brief maps a string to a unique integer value.
 * This ADT is thread safe and reentrant, but please
 * remember that you need to serialize access if you call multiple
 * functions. The typical example is:
 * @code
 * if(!stringmap_exists(...))
 *	  stringmap_add(...);
 * @endcode
 * Such code is dangerous in MT programs unless you lock access to the stringmap,
 * since another thread may add the same value inbetween the call
 * to the stringmap_exists() function and the stringmap_add() function.
 */

typedef struct stringmap_tag* stringmap;

stringmap stringmap_new(size_t nelem)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

void stringmap_free(stringmap sm);

/* Adds a new item to the stringmap */
status_t stringmap_add(stringmap sm, const char *s, unsigned long* pid)
    __attribute__((warn_unused_result));

/* Returns 1 if the string exists, else 0 */
bool stringmap_exists(stringmap sm, const char *s)
    __attribute__((warn_unused_result));

/*
 * Remove all entries from the stringmap, the stringmap itself is reusable.
 * Good to have if you want to refresh the cache.
 */
status_t stringmap_invalidate(stringmap sm)
    __attribute__((warn_unused_result));

/* Get id for a given string. Returns 1 if the string exists, else 0 */
status_t stringmap_get_id(stringmap sm, const char *s, unsigned long* pid)
    __attribute__((warn_unused_result));

/*
 * Walk the stringmap, calling the callback function once for
 * each element in the map. The callback must return 1 to continue
 * or 0 to stop walking.
 * The callback function will be called with the mapped value s and the user
 * proviced argument arg, which can be NULL.
 */
int stringmap_foreach(stringmap sm, int(*fn)(const char*s, void *arg), void *arg)
    __attribute__((warn_unused_result));

/*
 * Return a stringmap containing elements present in sm1 only.
 * If sm1 contains A,B,C,D and sm2 contains A,C,E,F then the
 * returned stringmap will contain B,D.
 *
 * Remember to free it after use.
 */
stringmap stringmap_subset(stringmap sm1, stringmap sm2)
    __attribute__((warn_unused_result))
    __attribute__((malloc))
    __attribute__((nonnull));

/* Convert a stringmap to a list. Strings will be copied  */
list stringmap_tolist(stringmap sm)
    __attribute__((warn_unused_result))
    __attribute__((malloc))
    __attribute__((nonnull));


#endif
