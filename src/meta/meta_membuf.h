/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef META_MEMBUF_H
#define META_MEMBUF_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief High speed memory buffering.
 *
 * We some places have memory buffers, which is a collection of
 * bytes with a fixed size. These buffers aren't zero terminated
 * in any way, which means that it can be quite easy to miscalculate
 * e.g. offsets or lengths when we access them.
 *
 * You can write data to the buffer and you can read data from the buffer,
 * assuming that there's data in the buffer.
 *
 * Note that the membuf isn't a ring buffer. If there are e.g. 100
 * bytes available for writing and you've written 100 bytes, for
 * then to read 50 bytes, this does not free up 50 bytes for writing.
 */

/* The membuf adt */
typedef struct membuf_tag *membuf;

struct membuf_tag {
	size_t size;

	/* These two member are used when reading and writing
	 * from/to the buffer. We always append data to the buffer
	 * and we always read from the start of the buffer. So
	 * Bytes available for reading is written - read, and bytes
	 * available for writing is size - written. If we try to
	 * write more than there's room for at the end of the buffer
	 * *and* written == read, then the membuf_write() function will
	 * do an automatic reset of the buffer and start writing
	 * from the beginning of the buffer.
	 */
	size_t written;
	size_t read;
	char *data;
};


/* Creates a new membuf buffer. The buffer will have room for \e size bytes. */
membuf membuf_new(size_t size);

/* Frees a membuf buffer.  */
void membuf_free(membuf m);

/*
 * Appends count bytes to the buffer. Returns the number of bytes
 * actually added to the buffer. If the returned value is less
 * than the count parameter, it means that the buffer was too small to
 * store the data.
 */
size_t membuf_write(membuf mb, const void *src, size_t count);

/*
 * Read up to count bytes from the buffer and place them in dest.
 * Returns the number of bytes read, or 0 if no data was available to read.
 */
size_t membuf_read(membuf, void *dest, size_t count);

/*
 * Return the number of bytes available for reading from the membuf buffer.
 */
static inline size_t membuf_canread(membuf mb)
{
	assert(mb != NULL);
	assert(mb->written >= mb->read);
	return mb->written - mb->read;
}

static inline void membuf_set_written(membuf mb, size_t cb)
{
	assert(mb->written == 0);
	mb->written = cb;
}

/* Return the number of bytes available for writing.  */
static inline size_t membuf_canwrite(membuf mb)
{
	assert(mb != NULL);

	/* Report full size if next write will reset anyway */
	if (mb->read == mb->written)
		return mb->size;
	else
		return mb->size - mb->written;
}

/* Empties the content of the buffer.  */
static inline void membuf_reset(membuf mb)
{
	assert(mb != NULL);
	mb->read = mb->written = 0;
}


/*
 * Ungets a previous read of one character. It will fail if the e.g. the
 * buffer has been explicitly or implicitly reset since
 * the character was read from the buffer. Since this is a
 * membuf, you can unget more than one character, but this
 * version supports one character only.
 *
 * Returns 1 if successful, else 0.
 */
static inline int membuf_unget(membuf mb)
{
	assert(mb != NULL);

	if (mb->read > 0) {
		mb->read--;
		return 1;
	}

	return 0;
}

/*
 * Returns a pointer to the data in the buffer. Nice to have
 * if you e.g. want to pass the contents of the buffer as an argument to
 * a different function like write(2). The buffer must contain some data.
 */
static inline void *membuf_data(membuf mb)
{
	assert(mb != NULL);
	return mb->data;
}

/*
 * Sets the entire content of the buffer to the character c.
 * This function does not change any internal status pointers,
 * and is excellent to use if you want to zero-terminate the
 * buffer and magically create a string. To do that, first set
 * the entire buffer to '\0', then write data to the buffer. Remember
 * not to write to the last byte in the buffer, since that will
 * overwrite the last '\0' added by membuf_set().
 */
static inline void membuf_set(membuf mb, int c)
{
	assert(mb != NULL);
	memset(mb->data, c, mb->size);
}

/*
 * Returns the total size of the buffer, which is the same
 * as the size parameter to the membuf_new() function.
 *
 * @see membuf_avail, membuf_used
 */
static inline size_t membuf_size(membuf mb)
{
	assert(mb != NULL);
	return mb->size;
}

#ifdef __cplusplus
}
#endif

#endif
