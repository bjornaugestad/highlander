/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */


#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "internals.h"

/* Stores info for one dynamic page */
struct dynamic_page_tag {
    cstring uri;
    page_attribute attr;
    PAGE_FUNCTION handler;
};

dynamic_page dynamic_new(const char* uri, PAGE_FUNCTION handler, page_attribute a)
{
    dynamic_page p;

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    p->handler = handler;
    p->attr = NULL;
    if ((p->uri = cstring_dup(uri)) == NULL) {
        free(p);
        return NULL;
    }

    if (a != NULL && (p->attr = attribute_dup(a)) == NULL) {
        cstring_free(p->uri);
        free(p);
        return NULL;
    }

    return p;
}

void dynamic_free(dynamic_page p)
{
    assert(p != NULL);

    if (p != NULL) {
        attribute_free(p->attr);
        cstring_free(p->uri);
        free(p);
    }
}

const char* dynamic_get_uri(dynamic_page p)
{
    assert(p != NULL);
    return c_str(p->uri);
}

status_t dynamic_set_uri(dynamic_page p, const char* value)
{
    assert(p != NULL);
    assert(value != NULL);

    return cstring_set(p->uri, value);
}

void dynamic_set_handler(dynamic_page p, PAGE_FUNCTION func)
{
    assert(p != NULL);
    assert(func != NULL);
    p->handler = func;
}

int dynamic_run(dynamic_page p, const http_request req, http_response response)
{
    assert(p != NULL);

    return (*p->handler)(req, response);
}

status_t dynamic_set_attributes(dynamic_page p, page_attribute a)
{
    attribute_free(p->attr);
    if ((p->attr = attribute_dup(a)) == NULL)
        return failure;

    return success;
}

page_attribute dynamic_get_attributes(dynamic_page p)
{
    assert(p != NULL);
    return p->attr;
}

#ifdef CHECK_DYNAMIC_PAGE

int dummy(http_request rq, http_response rsp)
{
    return 0;
}

int main(void)
{
    dynamic_page p;
    size_t i, niter;

    for (i = 0; i < niter; i++) {
        if ((p = dynamic_new("/dummy_uru", dummy, NULL)) == NULL)
            return 77;

        dynamic_page_free(p);
    }

    return 0;
}
#endif
