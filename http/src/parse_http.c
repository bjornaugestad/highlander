/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <assert.h>
#include <string.h>

#include <meta_common.h>
#include <meta_error.h>
#include <connection.h>

#include <http_request.h>
#include "internals.h"

/* Ungrouped handlers */
static status_t parse_connection(connection conn, const char *value)
{
    assert(conn != NULL);
    assert(value != NULL);

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
    status_t (*handler)(connection req, const char *value);
} connection_map[] = {
    { "connection",			parse_connection },
};


// Some of the properties received belongs to the connection object
// instead of the request object, as one connection may "live" longer
// than the request.
status_t parse_request_headerfield(connection conn, const char *name,
    const char *value, http_request req, error e)
{
    assert(name != NULL);
    assert(value != NULL);
    assert(req != NULL);
    assert(e != NULL);

    entity_header eh = request_get_entity_header(req);
    general_header gh = request_get_general_header(req);

    // Is it a general header field?
    int idx;
    if ((idx = find_general_header(name)) != -1)
        return parse_general_header(idx, gh, value, e);

    // Is it an entity header field?
    if ((idx = find_entity_header(name)) != -1)
        return parse_entity_header(idx, eh, value, e);

    // Now locate the handling function.
    // Go for the connection_map first as it is smaller
    size_t n = sizeof connection_map / sizeof *connection_map;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(name, connection_map[i].name) == 0)
            return connection_map[i].handler(conn, value);
    }

    if ((idx = find_request_header(name)) != -1)
        return parse_request_header(idx, req, value, e);

    // We have an unknown fieldname if we reach this point
    debug("%s: unknown header field: %s\n", __func__, name);

    // ignore the unknown field
    return success; 
}

status_t parse_response_headerfield(const char *name, const char *value,
    http_response req, error e)
{
    assert(name != NULL);
    assert(value != NULL);
    assert(req != NULL);
    assert(e != NULL);

    entity_header eh = response_get_entity_header(req);
    general_header gh = response_get_general_header(req);

    /* Is it a general header field? */
    int idx;
    if ((idx = find_general_header(name)) != -1)
        return parse_general_header(idx, gh, value, e);

    /* Is it an entity header field? */
    if ((idx = find_entity_header(name)) != -1)
        return parse_entity_header(idx, eh, value, e);

    if ((idx = find_response_header(name)) != -1)
        return parse_response_header(idx, req, value, e);

    // We have an unknown fieldname if we reach this point
    // Silently ignore the unknown field
    return success;
}

