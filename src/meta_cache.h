#ifndef META_CACHE_H
#define META_CACHE_H

#include <stddef.h>
#include <meta_common.h>

typedef struct cache_tag* cache;

cache cache_new(size_t nelem, size_t hotlist_nelem, size_t cb);
void cache_free(cache c, dtor cleanup);

int cache_add(cache c, size_t id, void* data, size_t cb, int pin);
int cache_exists(cache c, size_t id);
int cache_get(cache c, size_t id, void** pdata, size_t* pcb);
int cache_remove(cache c, size_t id);

#if 0
void cache_invalidate(cache c);
#endif


#endif

