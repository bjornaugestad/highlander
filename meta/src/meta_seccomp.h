#ifndef META_SECCOMP_H
#define META_SECCOMP_H

#include <seccomp.h> // do it to get the macros

// We wrap seccomp in a couple of functions and use
// void* as handle for scmp_filter_ctx, which is void* anyways.

void* drop_perms(const int *perms_to_keep) __attribute__((nonnull));
void  release_perms(void *vctx) __attribute__((nonnull));

#endif

