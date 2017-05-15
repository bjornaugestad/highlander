/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_CONFIGFILE_H
#define META_CONFIGFILE_H

#include <stdbool.h>
#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct configfile_tag* configfile;

configfile configfile_read(const char *path)
    __attribute__((malloc));

bool configfile_exists(configfile cf, const char *name)
    __attribute__((nonnull(1, 2)));

status_t configfile_get_string(configfile cf, const char *name,
    char *value, size_t cb)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

status_t configfile_get_long(configfile cf, const char *name, long *value)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

status_t configfile_get_ulong(configfile cf, const char *name,
    unsigned long *value)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

status_t configfile_get_int(configfile cf, const char *name, int *value)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));

status_t configfile_get_uint(configfile cf, const char *name,
    unsigned int *value)
    __attribute__((nonnull(1, 2, 3)))
    __attribute__((warn_unused_result));


void configfile_free(configfile cf);


#ifdef __cplusplus
}
#endif

#endif
