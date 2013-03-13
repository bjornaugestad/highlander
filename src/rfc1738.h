#ifndef RFC1738_H
#define RFC1738_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
#endif

/* @file
 * Implements encode and decode() functions for HTTP URL arguments
 * according to RFC 1738.
 *
 * The short and simple rule is that if a character is A-Za-z0-9
 * it is not encoded, anything else is encoded. The character is
 * encoded as a two digit hex number, prefixed with %. 
 *
 * Issues: This version decodes %00, which maps to '\0'.
 * Don't know if that's a serious issue or not(security).
 */

/* Changed to work on memory buffers instead of just strings */
size_t rfc1738_encode(char* dest, size_t cbdest, const char* src, size_t cbsrc);
size_t rfc1738_decode(char* dest, size_t cbdest, const char* src, size_t cbsrc);

/* Works for zero terminated strings, the result will be zero terminated too .
 * Returns length of dest string, excluding null character.
 */
size_t rfc1738_encode_string(char* dest, size_t cbdest, const char* src);
size_t rfc1738_decode_string(char* dest, size_t cbdest, const char* src);

#ifdef __cplusplus
}
#endif

#endif

