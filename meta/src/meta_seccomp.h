#ifndef META_SECCOMP_H
#define META_SECCOMP_H

// We wrap seccomp in a couple of functions and use
// void* as handle for scmp_filter_ctx, which is void* anyways.

// include  it to get the macros SCMP_SYS() so our callers
// can use it when setting up permission arrays.
#include <seccomp.h> 


void* meta_drop_perms(const int *perms_to_keep) __attribute__((nonnull));
void  meta_release_perms(void *vctx) __attribute__((nonnull));

#endif

