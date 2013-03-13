#ifndef META_ARRAY_H
#define META_ARRAY_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct array_tag* array;

array array_new(size_t nmemb, int can_grow);
void  array_free(array a, dtor cln);

size_t array_nelem(array a);
void*  array_get(array a, size_t ielem);
int    array_add(array a, void* elem);
int    array_extend(array a, size_t nmemb);

#ifdef __cplusplus
}
#endif

#endif 

