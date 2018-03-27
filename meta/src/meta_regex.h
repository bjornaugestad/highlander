#ifndef META_REGEX_H
#define META_REGEX_H

#include <stddef.h>
#include <stdbool.h>

typedef struct regex_tag *regex;
regex regex_new(void);
void regex_free(regex p);

bool regex_comp(regex p, const char *expr);

// Return number of matches, or -1 if an error occurred.
// Note that, depending on the expression, the returned number
// may need some interpretation. For example, "(foo)(bar)"
// and "foobar" will both match "xxfoobarxx", but the first
// expression will return 3, whereas the second will return 1.
// The reason for this is that each () is a submatch in
// extended POSIX regexp. The first has two, so regexec() 
// returns 3 values, the whole and the two subexpressions.
int regex_exec(regex p, const char *haystack);

// Get start and stop for expr and subexpr. The silly pointer names are to
// mimic the original struct member names in regmatch_t, which
// are rm_so and rm_eo. They're short for regmatch_startoffset and
// regmatch_endoffset. BIG NOTE: end offset is exclusive and is to
// the char *after* the match.
//
// Function will barf, loudly, on invalid index values. Should be
// in the range 0 <= regex_exec()'s return value. index 0 is for
// the full match and 1..n are for the submatches, if any.
void regex_get_match(regex p, int index, size_t *pso, size_t *peo);

// If buf is NULL and bufsize is 0, function returns the
// required buffer size to store the error message.
size_t regex_error(regex p, char *buf, size_t bufsize);

#endif
