#ifndef PAGE_ATTRIBUTE_H
#define PAGE_ATTRIBUTE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct page_attribute_tag*	page_attribute;

page_attribute attribute_new(void) __attribute__((malloc, warn_unused_result));
void attribute_free(page_attribute a);

status_t attribute_set_media_type   (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_language     (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_charset      (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_authorization(page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 
status_t attribute_set_encoding     (page_attribute a, const char *val) __attribute__((nonnull, warn_unused_result)); 

const char*	attribute_get_language  (page_attribute a) __attribute__((nonnull, warn_unused_result)); 
const char*	attribute_get_charset   (page_attribute a) __attribute__((nonnull, warn_unused_result)); 
const char*	attribute_get_encoding  (page_attribute a) __attribute__((nonnull, warn_unused_result)); 
const char*	attribute_get_media_type(page_attribute a) __attribute__((nonnull, warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif


