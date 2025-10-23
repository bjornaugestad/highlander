/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_MEMBUF_H
#define META_MEMBUF_H

#include <stddef.h>

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * High speed memory buffering.
 *
 * We some places have memory buffers, which is a collection of
 * bytes with a fixed size. These buffers aren't zero terminated
 * in any way, which means that it can be quite easy to miscalculate
 * e.g. offsets or lengths when we access them.
 *
 * You can write data to the buffer and you can read data from the
 * buffer, assuming that there's data in the buffer.
 *
 * Note that the membuf isn't a ring buffer. If there are e.g. 100
 * bytes available for writing and you've written 100 bytes, for
 * then to read 50 bytes, this does not free up 50 bytes for writing.
 * NOTE 20170515: Are we sure about this semantic? ring buffers
 * are nice... Well, I don't remember the rationale for the current
 * membuf semantics. I think the general idea is to use the membuf
 * to read chunks of data from e.g. a socket, and then read the
 * data while e.g parsing a protocol.
 */

/* The membuf adt */
typedef struct membuf_tag *membuf;

// Creates a new membuf buffer. The buffer will have room for size bytes.
membuf membuf_new(size_t size) __attribute__((malloc, warn_unused_result));

// Frees a membuf buffer.
void membuf_free(membuf this);

void membuf_freev(void *p);

// Return the number of bytes available for reading from the membuf buffer.
size_t membuf_canread(membuf mb) __attribute__((warn_unused_result, nonnull));

void membuf_set_written(membuf mb, size_t cb) __attribute__((nonnull));

// Return the number of bytes available for writing.
size_t membuf_canwrite(membuf mb) __attribute__((warn_unused_result, nonnull));

/* Empties the content of the buffer.  */
void membuf_reset(membuf mb) __attribute__((nonnull));

/*
 * Ungets a previous read of one character. It will fail if the e.g.
 * the buffer has been explicitly or implicitly reset since
 * the character was read from the buffer. Since this is a
 * membuf, you can unget more than one character, but this
 * version supports one character only.
 */
status_t membuf_unget(membuf mb) __attribute__((warn_unused_result, nonnull));

/*
 * Returns a pointer to the data in the buffer. Nice to have
 * if you e.g. want to pass the contents of the buffer as an argument
 * to a different function like write(2). The buffer must contain some
 * data.
 */
void* membuf_data(membuf mb) __attribute__((warn_unused_result, nonnull));

// Sets the entire content of the buffer to the character c. This function does
// not change any internal status pointers, and is useful if you want to
// zero-terminate the buffer and magically create a string. To do that, first
// set the entire buffer to '\0', then write data to the buffer. Remember not to
// write to the last byte in the buffer, since that will overwrite the last '\0'
// added by membuf_set().
void membuf_set(membuf mb, int c) __attribute__((nonnull));

// Returns the total size of the buffer, which is the same
// as the size parameter to the membuf_new() function.
size_t membuf_size(membuf mb) __attribute__((warn_unused_result, nonnull));

// Read up to count bytes from the buffer and place them in dest.
// Returns the number of bytes read, or 0 if no data was available to read.
size_t membuf_read(membuf this, void *dest, size_t count) __attribute__((warn_unused_result, nonnull));

// Appends count bytes to the buffer. Returns the number of bytes actually added
// to the buffer. If the returned value is less than the count parameter, it
// means that the buffer was too small to store the data.
size_t membuf_write(membuf this, const void *src, size_t count) __attribute__((warn_unused_result, nonnull));

#ifdef __cplusplus
}
#endif

#endif
