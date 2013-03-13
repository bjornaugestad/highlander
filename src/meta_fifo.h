#ifndef META_FIFO_H
#define META_FIFO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fifo_tag* fifo;

fifo fifo_new(size_t size);
void fifo_free(fifo p, dtor dtor_fn);

int fifo_lock(fifo p);
int fifo_unlock(fifo p);
int fifo_add(fifo p, void* data);

size_t fifo_nelem(fifo p);
size_t fifo_free_slot_count(fifo p);

void* fifo_get(fifo p);
void* fifo_peek(fifo p, size_t i);

int fifo_write_signal(fifo p, void* data);
int fifo_wait_cond(fifo p);
int fifo_wake(fifo p);
int fifo_signal(fifo p);

#ifdef __cplusplus
}
#endif

#endif /* guard */

