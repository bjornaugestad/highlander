#ifndef GENERAL_HEADER_H
#define GENERAL_HEADER_H

#include <stddef.h>

#include <connection.h>
#include <meta_error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct general_header_tag *general_header;

general_header general_header_new(void);
/*
 * The general header fields are shared between http requests and http
 * responses. The fields are described in RFC 2616, section 4.5.
 * This function itself returns -1 if the field was NOT a general header field
 */
int find_general_header(const char *name);
status_t parse_general_header(int idx, general_header gh, const char *value, error e) __attribute__((nonnull, warn_unused_result));

void general_header_free(general_header p);
void general_header_recycle(general_header p);
status_t general_header_send_fields(general_header gh, connection c) __attribute__((nonnull, warn_unused_result));

void general_header_dump(general_header gh, void *file) __attribute__((nonnull));
void general_header_set_no_cache(general_header gh);
void general_header_set_no_store(general_header gh);
void general_header_set_max_age(general_header gh, int value);
void general_header_set_s_maxage(general_header gh, int value);
void general_header_set_max_stale(general_header gh, int value);
void general_header_set_min_fresh(general_header gh, int value);
void general_header_set_no_transform(general_header gh);
void general_header_set_only_if_cached(general_header gh);

void general_header_set_private(general_header gh);
void general_header_set_public(general_header gh);
void general_header_set_must_revalidate(general_header gh);
void general_header_set_proxy_revalidate(general_header gh);

void general_header_set_date(general_header gh, time_t value);
status_t general_header_set_connection(general_header gh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t general_header_set_pragma(general_header gh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t general_header_set_trailer(general_header gh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t general_header_set_transfer_encoding(general_header gh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t general_header_set_upgrade(general_header gh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t general_header_set_via(general_header gh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t general_header_set_warning(general_header gh, const char *value) __attribute__((nonnull, warn_unused_result));

bool general_header_get_no_cache(general_header gh);
bool general_header_get_no_store(general_header gh);
int general_header_get_max_age(general_header gh);
int general_header_get_s_maxage(general_header gh);
int general_header_get_max_stale(general_header gh);
int general_header_get_min_fresh(general_header gh);
bool general_header_get_no_transform(general_header gh);
bool general_header_get_only_if_cached(general_header gh);
bool general_header_get_public(general_header gh);
bool general_header_get_private(general_header gh);
bool general_header_get_must_revalidate(general_header gh);
bool general_header_get_proxy_revalidate(general_header gh);


time_t      general_header_get_date(general_header gh);
const char* general_header_get_connection(general_header gh);
const char* general_header_get_trailer(general_header gh);
const char* general_header_get_transfer_encoding(general_header gh);
const char* general_header_get_upgrade(general_header gh);
const char* general_header_get_via(general_header gh);
const char* general_header_get_warning(general_header gh);

/* Tests if a property is set or not. Use it before
 * calling the _get() functions */
bool general_header_no_cache_isset(general_header gh);
bool general_header_no_store_isset(general_header gh);
bool general_header_max_age_isset(general_header gh);
bool general_header_connection_isset(general_header gh);
bool general_header_pragma_isset(general_header gh);
bool general_header_max_stale_isset(general_header gh);
bool general_header_min_fresh_isset(general_header gh);
bool general_header_no_transform_isset(general_header gh);
bool general_header_only_if_cached_isset(general_header gh);
bool general_header_date_isset(general_header gh);
bool general_header_trailer_isset(general_header gh);
bool general_header_transfer_encoding_isset(general_header gh);
bool general_header_upgrade_isset(general_header gh);
bool general_header_via_isset(general_header gh);
bool general_header_warning_isset(general_header gh);
bool general_header_is_chunked_message(general_header gh);


#ifdef __cplusplus
}
#endif

#endif
