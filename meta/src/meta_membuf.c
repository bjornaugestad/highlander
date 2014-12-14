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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_membuf.h>

membuf membuf_new(size_t size)
{
	membuf p;

	assert(size > 0);

	if ((p = malloc(sizeof *p)) == NULL)
		return NULL;

	if ((p->data = malloc(size)) == NULL) {
		free(p);
		return NULL;
	}

	p->size = size;
	p->written = p->read = 0;

	return p;
}

void membuf_free(membuf this)
{
	if (this != NULL) {
		free(this->data);
		free(this);
	}
}

size_t membuf_write(membuf this, const void *src, size_t count)
{
	size_t n;

	assert(this != NULL);
	assert(src != NULL);

	/* Don't bother to write empty buffers */
	if (count == 0)
		return 0;

	/*
	 * Decide how much we can write and reset the
	 * buffer if needed (and possible).
	 */
	if (count > this->size - this->written) {
		/*
		 * Has all written bytes also been read?
		 * If so, reset the buffer.
		 */
		if (this->written == this->read)
			this->written = this->read = 0;

		/* Is there space available after reset ? */
		if (count <= this->size - this->written)
			n = count;
		else
			n = this->size - this->written;
	}
	else
		n = count;

	/* Check that we didn't screw up above */
	assert(n <= membuf_canwrite(this));

	memcpy(&this->data[this->written], src, n);
	this->written += n;

	return n;
}

size_t membuf_read(membuf this, void *dest, size_t count)
{
	size_t navail;

	assert(this != NULL);
	assert(dest != NULL);
	assert(count != 0);
	assert(this->written >= this->read);

	navail = membuf_canread(this);
	if (navail < count)
		count = navail;

	if (count == 0)
		return 0;

	memcpy(dest, &this->data[this->read], count);
	this->read += count;

	assert(this->read <= this->written);

	/* Reset offset counters if all bytes written also have been read */
	if (this->written == this->read)
		this->written = this->read = 0;

	return count;
}

#ifdef CHECK_MEMBUF

int main(void)
{
#define BUFSIZE 10000UL

	size_t cb, i, nelem = 100, size = BUFSIZE;
	membuf mb;

	/* Create a dummy buffer and fill it with something */
	char readbuf[BUFSIZE];
	char writebuf[BUFSIZE];

	memset(writebuf, 'A', BUFSIZE);

	for (i = 0; i < nelem; i++) {
		mb = membuf_new(size);
		cb = membuf_write(mb, writebuf, BUFSIZE);
		assert(cb == BUFSIZE);

		cb = membuf_read(mb, readbuf, BUFSIZE);
		assert(cb == BUFSIZE);
		assert(memcmp(readbuf, writebuf, BUFSIZE) == 0);

		cb = membuf_read(mb, readbuf, BUFSIZE);
		assert(cb == 0);

		/* This should automatically reset the buffer */
		cb = membuf_write(mb, writebuf, BUFSIZE);
		assert(cb == BUFSIZE);

		cb = membuf_read(mb, readbuf, BUFSIZE);
		assert(cb == BUFSIZE);

		membuf_free(mb);
	}

	/* Now loop with odd sizes to see if we miss a byte or two.
	 * Size stuff so that we write less than we read, and that
	 * size%writes != 0.
	 */
	mb = membuf_new(23);
	cb = membuf_write(mb, writebuf, 7);
	assert(cb == 7);
	cb = membuf_write(mb, writebuf, 7);
	assert(cb == 7);
	cb = membuf_write(mb, writebuf, 7);
	assert(cb == 7);
	cb = membuf_write(mb, writebuf, 7);
	assert(cb == 2);

	cb = membuf_read(mb, readbuf, 6);
	assert(cb == 6);
	cb = membuf_read(mb, readbuf, 6);
	assert(cb == 6);
	cb = membuf_read(mb, readbuf, 6);
	assert(cb == 6);
	cb = membuf_read(mb, readbuf, 6);
	assert(cb == 5);


	/* Now write 15, read 10, then write 19, cb should then be 18 */
	membuf_reset(mb);
	cb = membuf_write(mb, writebuf, 15);
	assert(cb == 15);

	cb = membuf_read(mb, readbuf, 10);
	assert(cb == 10);

	cb = membuf_write(mb, writebuf, 19);
	assert(cb == 8);

	cb = membuf_read(mb, readbuf, 100);
	assert(cb == 13);

	membuf_free(mb);
	return 0;
}
#endif
