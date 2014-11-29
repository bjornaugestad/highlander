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
#include <string.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_membuf.h>

membuf membuf_new(size_t size)
{
	membuf p;

	assert(size > 0);

	if ((p = malloc(sizeof *p)) == NULL
	|| (p->data = malloc(size)) == NULL) {
		free(p);
		p = NULL;
	}
	else {
		p->size = size;
		p->written = p->read = 0;
	}

	return p;
}

void membuf_free(membuf p)
{
	if (p != NULL) {
		free(p->data);
		free(p);
	}
}

size_t membuf_write(membuf p, const void *src, size_t cb)
{
	size_t cbToAdd;

	assert(p != NULL);
	assert(src != NULL);

	/* Don't bother to write empty buffers */
	if (cb == 0)
		return 0;
	/*
	 * Decide how much we can write and reset the
	 * buffer if needed (and possible).
	 */
	if (cb > (p->size - p->written)) {
		/*
		 * Has all written bytes also been read?
		 * If so, reset the buffer.
		 */
		if (p->written == p->read)
			p->written = p->read = 0;

		/* Is there space available after reset ? */
		if (cb <= (p->size - p->written))
			cbToAdd = cb;
		else
			cbToAdd = p->size - p->written;
	}
	else
		cbToAdd = cb;

	/* Check that we didn't screw up above */
	assert(cbToAdd <= membuf_canwrite(p));

	memcpy(&p->data[p->written], src, cbToAdd);
	p->written += cbToAdd;

	return cbToAdd;
}

size_t membuf_read(membuf p, void *dest, size_t cb)
{
	size_t cbAvail, cbToRead;

	assert(p != NULL);
	assert(dest != NULL);
	assert(cb != 0);
	assert(p->written >= p->read);

	cbAvail = membuf_canread(p);
	if (cbAvail >= cb)
		cbToRead = cb;
	else
		cbToRead = cbAvail;

	if (cbToRead == 0)
		return 0;

	memcpy(dest, &p->data[p->read], cbToRead);
	p->read += cbToRead;

	assert(p->read <= p->written);

	/* Reset offset counters if all bytes written also have been read */
	if (p->written == p->read)
		p->written = p->read = 0;

	return cbToRead;
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
