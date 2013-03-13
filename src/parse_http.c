
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <meta_misc.h>

#include "internals.h"


/* Ungrouped handlers */
static int parse_connection(connection conn, const char* s, meta_error e);

/*
 * Here we map header fields to handling functions. We need separate functions
 * for each version of HTTP as there may be differences between each version.
 * This is v1, so I keep it simple. It may be a little slow, though...
 */
static const struct connection_mapper {
	const char* name;
	int (*handler)(connection req, const char* value, meta_error e);
} connection_map[] = {
	{ "connection",			parse_connection },
};

/*
 * Some of the properties received belongs to the connection object
 * instead of the request object, as one connection may "live" longer
 * than the request.
 */
int parse_request_headerfield(
	connection conn,
	const char* name,
	const char* value,
	http_request req, 
	meta_error e)
{
	size_t i, size;
	int idx;

	entity_header eh = request_get_entity_header(req);
	general_header gh = request_get_general_header(req);

	/* Is it a general header field? */
	if( (idx = find_general_header(name)) != -1)
		return parse_general_header(idx, gh, value, e);

	/* Is it an entity header field? */
	if( (idx = find_entity_header(name)) != -1)
		return parse_entity_header(idx, eh, value, e);

	/* Now locate the handling function.
	 * Go for the connection_map first as it is smaller */
	size = sizeof(connection_map) / sizeof(connection_map[0]);
	for(i = 0; i < size; i++) {
		if(0 == strcmp(name, connection_map[i].name)) {
			/* execute the handling function and return */
			return connection_map[i].handler(conn, value, e);
		}
	}

	if( (idx = find_request_header(name)) != -1)
		return parse_request_header(idx, req, value, e);

	/* We have an unknown fieldname if we reach this point */
	#if 0
	fprintf(stderr, "Ignoring unknown request header field:%s, value %s\n", name, value);
	#endif
	return 1; /* Silently ignore the unknown field */
}

int parse_response_headerfield(
	const char* name,
	const char* value,
	http_response req, 
	meta_error e)
{
	int idx;

	fprintf(stderr, "Got name/value %s/%s\n", name, value);
	entity_header eh = response_get_entity_header(req);
	general_header gh = response_get_general_header(req);

	/* Is it a general header field? */
	if( (idx = find_general_header(name)) != -1)
		return parse_general_header(idx, gh, value, e);

	/* Is it an entity header field? */
	if( (idx = find_entity_header(name)) != -1)
		return parse_entity_header(idx, eh, value, e);

	if( (idx = find_response_header(name)) != -1)
		return parse_response_header(idx, req, value, e);

	/* We have an unknown fieldname if we reach this point */
	#if 0
	fprintf(stderr, "Ignoring unknown response header field:%s\n", name);
	#endif
	return 1; /* Silently ignore the unknown field */
}


static int parse_connection(connection conn, const char* value, meta_error e)
{
	assert(NULL != value);
	UNUSED(e);

	if(strstr(value, "keep-alive"))
		connection_set_persistent(conn, 1);

	if(strstr(value, "close"))
		connection_set_persistent(conn, 0);

	return 1;
}

/* Helper function to have the algorithm one place only */
int parse_multivalued_fields(
	void *dest,
	const char* value,
	int(*set_func)(void* dest, const char* value, meta_error e),
	meta_error e)
{
	const int sep = (int)',';
	char buf[100];
	char* s;

	assert(NULL != dest);
	assert(NULL != value);

	while( (s = strchr(value, sep)) != NULL) {
		/* The correct type would be ptrdiff_t, but -ansi -pedantic complains */
		size_t span = (size_t)(s - value);
		if(span + 1 > sizeof buf) {
			/* We don't want buffer overflow... */
			value = s + 1;
			continue;
		}

		memcpy(buf, value, span);
		buf[span] = '\0';
		if(!set_func(dest, buf, e))
			return 0;

		value = s + 1;
	}

	return set_func(dest, value, e);
}
