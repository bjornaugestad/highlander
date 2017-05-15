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

    if ((this = malloc(sizeof *this)) == NULL
    || (this->data = malloc(size)) == NULL) {
        free(this);
        return NULL;
    }

    this->size = size;
    this->nwritten = this->nread = 0;

    return this;
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
