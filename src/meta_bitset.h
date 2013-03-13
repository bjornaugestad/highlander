#ifndef META_BITSET_H
#define META_BITSET_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bitset_tag* bitset;

bitset bitset_new(size_t bitcount);
void bitset_free(bitset b);

void bitset_set(bitset b, size_t i);
void bitset_clear(bitset b, size_t i);
void bitset_clear_all(bitset b);
void bitset_set_all(bitset b);

int bitset_is_set(bitset b, size_t i);

size_t bitset_size(bitset b);

bitset bitset_map(void* mem, size_t cb);
void bitset_remap(bitset b, void* mem, size_t cb);
void bitset_unmap(bitset b);
void* bitset_data(bitset b);

#ifdef __cplusplus
}
#endif

#endif /* guard */

