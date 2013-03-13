#ifndef META_POOL_H
#define META_POOL_H

#include <stddef.h> /* for size_t */

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct pool_tag* pool;

pool pool_new(size_t nelem);
void pool_free(pool p, dtor cleanup);

void  pool_add(pool p, void* resource);
void* pool_get(pool p);
void  pool_recycle(pool p, void* resource);

#ifdef __cplusplus
}
#endif

#endif /* guard */

