/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_PROCESS_H
#define META_PROCESS_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct process_tag* process;

process process_new(const char *appname)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

void process_free(process p);

int process_shutting_down(process p)
    __attribute__((nonnull));

status_t process_set_rootdir(process p, const char *path)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t process_set_username(process p, const char *username)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));


status_t process_add_object_to_start(
    process p,
    void *object,
    status_t do_func(void *),
    status_t undo_func(void *),
    status_t run_func(void *),
    status_t shutdown_func(void *))
    __attribute__((warn_unused_result))
    __attribute__((nonnull(1, 2)));


status_t process_start(process p, int fork_and_close)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

status_t process_wait_for_shutdown(process p)
    __attribute__((warn_unused_result))
    __attribute__((nonnull));

int process_get_exitcode(process p, void *object)
    __attribute__((nonnull));


#ifdef __cplusplus
}
#endif

#endif
