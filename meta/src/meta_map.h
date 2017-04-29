/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_MAP_H
#define META_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <meta_common.h>

/*
 * @file
 * I need a way to associate some value with a name, also known as a map.
 * This class is reentrant like the others.
 */

/* The map adt */
typedef struct map_tag* map;

/* The map iterator */
typedef struct map_iterator_tag {
    void *node;
} map_iterator;

/*
 * Creates a new map.
 *
 * The function pointer argument points to a function suitable
 * of freeing the value argument. If it is NULL, no memory will
 * be freed.
 */
map map_new(dtor free_fn) __attribute__((malloc));

/*
 * Deletes a map.
 */
void map_free(map m);

/*
 * Creates a new entry or updates an existing entry.
 * Returns success on OK and failure if out of memory.
 * If an existing entry exists, the memory pointed to by the prev.
 * value will be freed.
 */
status_t map_set(map m, const char *key, void *value)
    __attribute__((nonnull(1, 2)))
    __attribute__((warn_unused_result));

/*
 * Returns true if key exists in map, false if not.
 */
bool map_exists(map m, const char *key)
    __attribute__((nonnull(1, 2)));

void *map_get(map m, const char *key)
    __attribute__((nonnull(1, 2)));

/*
 * Deletes an entry from the map. Any data will also be deleted.
 * Returns 0 on success, ENOENT if no entry exists.
 */
int map_delete(map m, const char *key)
    __attribute__((nonnull(1, 2)));

/*
 * Executes function f once for each member in the map. Function f should
 * treat the parameters as read only if the data will be used later.
 *
 * The function f should return non-zero if the iteration should continue
 * and return 0 if it wants the iteration to stop.
 *
 * map_foreach returns 0 if the iteration was aborted and 1 if it wasn't.
 */
int map_foreach(map m, void *args, int(*f)(void *args, char *key, void *data))
    __attribute__((nonnull(1)));

map_iterator map_first(map m)
    __attribute__((nonnull(1)));

map_iterator map_next(map_iterator mi);
char *map_key(map_iterator mi);
void *map_value(map_iterator mi);

/*
 * returns 1 if no more entries exist. map_next(mi) would then return NULL.
 */
int map_end(map_iterator mi);


#ifdef __cplusplus
}
#endif

#endif
