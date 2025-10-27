#ifndef ENTITY_HEADER_H
#define ENTITY_HEADER_H

#include <time.h>

#include <meta_common.h>
#include <meta_error.h>
#include <connection.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct entity_header_tag *entity_header;

/* Return an index in the entity header array, or -1 if the field was not found. */
int find_entity_header(const char *name);
status_t parse_entity_header(int idx, entity_header gh, const char *value, error e) __attribute__((nonnull, warn_unused_result));

entity_header entity_header_new(void);
void entity_header_free(entity_header p);
void entity_header_recycle(entity_header p);
status_t entity_header_send_fields(entity_header eh, connection c) __attribute__((nonnull, warn_unused_result));

status_t entity_header_set_allow(entity_header eh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t entity_header_set_content_encoding(entity_header eh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t entity_header_set_content_language(entity_header eh, const char *value, error e) __attribute__((nonnull, warn_unused_result));
void entity_header_set_content_length(entity_header eh, size_t value);
status_t entity_header_set_content_location(entity_header eh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t entity_header_set_content_md5(entity_header eh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t entity_header_set_content_range(entity_header eh, const char *value) __attribute__((nonnull, warn_unused_result));
status_t entity_header_set_content_type(entity_header eh, const char *value) __attribute__((nonnull, warn_unused_result));
void entity_header_set_expires(entity_header eh, time_t value);
void entity_header_set_last_modified(entity_header eh, time_t value);

bool entity_header_content_type_is(entity_header eh, const char *val);

const char* entity_header_get_allow(entity_header eh);
const char* entity_header_get_content_encoding(entity_header eh);
const char* entity_header_get_content_language(entity_header eh);
size_t entity_header_get_content_length(entity_header eh);
const char* entity_header_get_content_location(entity_header eh);
const char* entity_header_get_content_md5(entity_header eh);
const char* entity_header_get_content_range(entity_header eh);
const char* entity_header_get_content_type(entity_header eh);
time_t entity_header_get_expires(entity_header eh);
time_t entity_header_get_last_modified(entity_header eh);

bool entity_header_allow_isset(entity_header eh);
bool entity_header_content_encoding_isset(entity_header eh);
bool entity_header_content_language_isset(entity_header eh);
bool entity_header_content_length_isset(entity_header eh);
bool entity_header_content_location_isset(entity_header eh);
bool entity_header_content_md5_isset(entity_header eh);
bool entity_header_content_range_isset(entity_header eh);
bool entity_header_content_type_isset(entity_header eh);
bool entity_header_expires_isset(entity_header eh);
bool entity_header_last_modified_isset(entity_header eh);

void entity_header_dump(entity_header gh, void *file)   __attribute__((nonnull));

#ifdef __cplusplus
}
#endif

#endif
