#ifndef META_JSON_H
#define META_JSON_H

#include <stddef.h>

#include "meta_list.h"

// TODO: Figure out a way to return meaningful errors. Do we want
// to use meta_error.h? Not sure, really. It's been a life time since
// I looked at it.

struct value;
struct value* json_parse(const void *src, size_t srclen) __attribute__((warn_unused_result));

// Free memory used by the list returned from json_parse().
void json_free(struct value *objects);

#endif

