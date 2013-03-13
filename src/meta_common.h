#ifndef META_COMMON_H
#define META_COMMON_H


#ifdef __cplusplus
extern "C" {
#endif

/* stuff common to all modules.  */

/*
 * A destructor (dtor) is a pointer to a function which frees
 * an object and all memory allocated by that object. We use dtor's
 * quite a lot, e.g. to destroy list members, array members and pool members.
 */
typedef void(*dtor)(void*);

/* Memory allocation. 
 * We use these functions to handle memory instead of the standard
 * functions. Why? We want to be able to set process wide error handling
 * for memory problems.
 * Use mem_set_error_handler() to set a callback function called when
 * we're out of memory. No need for fancy tracing of leaks and
 * allocated mem as valgrind solves that for us.
 */
#include <stddef.h>

void *mem_malloc(size_t cb);
void *mem_calloc(size_t nelem, size_t size);
void *mem_realloc(void* ptr, size_t size);
char* mem_strdup(const char* src);
void  mem_free(void *);
void  mem_set_error_handler(void(*handler)(void));

/* What to do when an error occurs. */
#define MEM_POLICY_NONE   0x00 /* Do nothing, that's the default  */
#define MEM_POLICY_ABORT  0x01 /* call abort(), good for debugging */
void  mem_set_error_policy(int policy);


/* We always need to be able to trace stuff, mainly for debugging.
 * Even if we think that everything is working fine, shit happens.
 * We therefore trace all functions called, including tid (if any) 
 * We always support the following global variables:
 * int meta_verbose_level. 0 means no output
 * int meta_indent_level. For output
 * These variables are always present so that command line processing
 * can be simpler. They just don't do anything in release builds (NDEBUG).
 */
extern int meta_verbose_level;
extern int meta_indent_level;

void verbose(int level, const char *fmt, ...);



#ifdef __cplusplus
}
#endif

#endif /* guard */

