#ifndef META_JSON_H
#define META_JSON_H

#include <stddef.h>

#include "meta_list.h"

struct json_parser;
struct value;

struct json_parser *json_parser_new(const void *src, size_t srclen)
    __attribute__((malloc))
    __attribute__((warn_unused_result));

void json_parser_free(struct json_parser *p);
struct value* json_parser_values(struct json_parser *p);

int json_parser_errcode(const struct json_parser *p);
const char* json_parser_errtext(const struct json_parser *p);

status_t json_parse(struct json_parser *p)
    __attribute__((warn_unused_result));

// Free memory used by the list returned from json_parse().
void json_free(struct value *objects);

#endif

