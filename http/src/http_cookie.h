#ifndef HTTP_COOKIE_H
#define HTTP_COOKIE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic value for cookies */
#define MAX_AGE_NOT_SET -1


typedef struct cookie_tag *cookie;

/* Cookies */
cookie cookie_new(void) __attribute__((malloc));

status_t cookie_set_name(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_value(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_comment(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_domain(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 
status_t cookie_set_path(cookie c, const char *s) __attribute__((nonnull, warn_unused_result)); 

void cookie_set_max_age(cookie c, int value) __attribute__((nonnull)); 
void cookie_set_version(cookie c, int value) __attribute__((nonnull)); 
void cookie_set_secure(cookie c, int value) __attribute__((nonnull)); 
const char*	cookie_get_name(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_value(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_comment(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_domain(cookie c)
    __attribute__((nonnull, warn_unused_result));

const char*	cookie_get_path(cookie c)
    __attribute__((nonnull, warn_unused_result));

int cookie_get_version(cookie c)
    __attribute__((nonnull, warn_unused_result));

int cookie_get_secure(cookie c)
    __attribute__((nonnull, warn_unused_result));

int cookie_get_max_age(cookie c)
    __attribute__((nonnull, warn_unused_result));


#ifdef __cplusplus
}
#endif

#endif
