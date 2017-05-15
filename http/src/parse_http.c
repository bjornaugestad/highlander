/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <meta_common.h>

#include "internals.h"

/* Ungrouped handlers */
static status_t parse_connection(connection conn, const char *value, error e)
{
    assert(value != NULL);
    UNUSED(e);

    if (strstr(value, "keep-alive"))
        connection_set_persistent(conn, 1);

    if (strstr(value, "close"))
        connection_set_persistent(conn, 0);

    return success;
}

/*
 * Here we map header fields to handling functions. We need separate functions
 * for each version of HTTP as there may be differences between each version.
 * This is v1, so I keep it simple. It may be a little slow, though...
 */
static const struct connection_mapper {
    const char *name;
    status_t (*handler)(connection req, const char *value, error e);
} connection_map[] = {
    { "connection",			parse_connection },
};

/*
 * Some of the properties received belongs to the connection object
 * instead of the request object, as one connection may "live" longer
 * than the request.
 */
status_t parse_request_headerfield(
    connection conn,
    const char *name,
    const char *value,
    http_request req,
    error e)
{
    size_t i, n;
    int idx;

    entity_header eh = request_get_entity_header(req);
    general_header gh = request_get_general_header(req);

    /* Is it a general header field? */
    if ((idx = find_general_header(name)) != -1)
        return parse_general_header(idx, gh, value, e);

    /* Is it an entity header field? */
    if ((idx = find_entity_header(name)) != -1)
        return parse_entity_header(idx, eh, value, e);

    /* Now locate the handling function.
     * Go for the connection_map first as it is smaller */
    n = sizeof connection_map / sizeof *connection_map;
    for (i = 0; i < n; i++) {
        if (strcmp(name, connection_map[i].name) == 0)
            return connection_map[i].handler(conn, value, e);
    }

    if ((idx = find_request_header(name)) != -1)
        return parse_request_header(idx, req, value, e);

    /* We have an unknown fieldname if we reach this point */
    debug("%s: unknown header field: %s\n", __func__, name);
    return success; /* Silently ignore the unknown field */
}

status_t parse_response_headerfield(
    const char *name,
    const char *value,
    http_response req,
    error e)
{
    int idx;

    entity_header eh = response_get_entity_header(req);
    general_header gh = response_get_general_header(req);

    /* Is it a general header field? */
    if ((idx = find_general_header(name)) != -1)
        return parse_general_header(idx, gh, value, e);

    /* Is it an entity header field? */
    if ((idx = find_entity_header(name)) != -1)
        return parse_entity_header(idx, eh, value, e);

    if ((idx = find_response_header(name)) != -1)
        return parse_response_header(idx, req, value, e);

    /* We have an unknown fieldname if we reach this point */
    return success; /* Silently ignore the unknown field */
}


/* Helper function to have the algorithm one place only */
int parse_multivalued_fields(
    void *dest,
    const char *value,
    int(*set_func)(void* dest, const char *value, error e),
    error e)
{
    const int sep = ',';
    char buf[100];
    char *s;

    assert(dest != NULL);
    assert(value != NULL);

    while ((s = strchr(value, sep)) != NULL) {
        /* The correct type would be ptrdiff_t,
         * but -ansi -pedantic complains */
        size_t span = (size_t)(s - value);
        if (span + 1 > sizeof buf) {
            /* We don't want buffer overflow... */
            value = s + 1;
            continue;
        }

        memcpy(buf, value, span);
        buf[span] = '\0';
        if (!set_func(dest, buf, e))
            return 0;

        value = s + 1;
    }

    return set_func(dest, value, e);
}
