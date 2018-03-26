#ifndef META_REGEX_H
#define META_REGEX_H

#include <stddef.h>

typedef struct regex_tag *regex;
regex regex_new(void);
void regex_free(regex p);

status_t regex_comp(regex p, const char *expr);

// If buf is NULL and bufsize is 0, function returns the
// required buffer size to store the error message.
size_t regex_error(regex p, char *buf, size_t bufsize);

#endif
