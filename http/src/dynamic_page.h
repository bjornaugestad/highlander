#ifndef DYNAMIC_PAGE_H
#define DYNAMIC_PAGE_H

#include <page_attribute.h>
#include <http_request.h>
#include <http_response.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dynamic_page_tag *dynamic_page;

dynamic_page dynamic_new(const char *uri, int (*handlerfn)(http_request, http_response), page_attribute a);
void dynamic_free(dynamic_page p);
void dynamic_set_handler(dynamic_page p, int (*handlerfn)(http_request, http_response));
status_t dynamic_set_uri(dynamic_page p, const char *value)
    __attribute__((nonnull, warn_unused_result));
int dynamic_run(dynamic_page p, const http_request, http_response);
status_t dynamic_set_attributes(dynamic_page p, page_attribute a)
    __attribute__((nonnull, warn_unused_result));
page_attribute dynamic_get_attributes(dynamic_page p);
const char*	dynamic_get_uri(dynamic_page p);


#ifdef __cplusplus
}
#endif

#endif
