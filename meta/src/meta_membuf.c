/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_membuf.h>

membuf membuf_new(size_t size)
{
    membuf this;

    assert(size > 0);

    if ((this = malloc(sizeof *this)) == NULL)
        return NULL;

    if ((this->data = malloc(size)) == NULL) {
        free(this);
        return NULL;
    }

    this->size = size;
    this->nwritten = this->nread = 0;

    return this;
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
    size_t navail;

    assert(this != NULL);
    assert(src != NULL);

    // Don't bother to write empty buffers
    if (count == 0)
        return 0;

    // Decide how much we can write and reset the
    // buffer if needed (and possible).
    navail = membuf_canwrite(this);
    if (count > navail)
        count = navail;

    memcpy(&this->data[this->nwritten], src, count);
    this->nwritten += count;

    return count;
}

size_t membuf_read(membuf this, void *dest, size_t count)
{
    size_t navail;

    assert(this != NULL);
    assert(dest != NULL);
    assert(count != 0);
    assert(this->nwritten >= this->nread);

    navail = membuf_canread(this);
    if (navail < count)
        count = navail;

    if (count == 0)
        return 0;

    memcpy(dest, &this->data[this->nread], count);
    this->nread += count;

    assert(this->nread <= this->nwritten);

    // Reset offset counters if all bytes written also have been read
    if (this->nwritten == this->nread)
        this->nwritten = this->nread = 0;

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
