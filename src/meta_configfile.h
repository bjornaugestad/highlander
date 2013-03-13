#ifndef META_CONFIGFILE_H
#define META_CONFIGFILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct configfile_tag* configfile;

configfile configfile_read(const char *path);

int configfile_exists(configfile cf, const char* name);
int configfile_get_string(configfile cf, const char *name, char *value, size_t cb);
int configfile_get_long(configfile cf, const char *name, long *value);
int configfile_get_ulong(configfile cf, const char *name, unsigned long *value);
int configfile_get_int(configfile cf, const char *name, int *value);
int configfile_get_uint(configfile cf, const char *name, unsigned int *value);

void configfile_free(configfile cf);


#ifdef __cplusplus
}
#endif

#endif

