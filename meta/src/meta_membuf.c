/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <meta_membuf.h>

/*
 * The two members written and read are used when reading and writing
 * from/to the buffer. We always append data to the buffer
 * and we always read from the start of the buffer. So
 * Bytes available for reading is written - read, and bytes
 * available for writing is size - written. If we try to
 * write more than there's room for at the end of the buffer
 * *and* written == read, then the membuf_write() function will
 * do an automatic reset of the buffer and start writing
 * from the beginning of the buffer.
 */
struct membuf_tag {
    size_t size;
    size_t nwritten;
    size_t nread;
    char *data;
};

membuf membuf_new(size_t size)
{
    membuf new;

    assert(size > 0);

    if ((new = malloc(sizeof *new)) != NULL
    && (new->data = malloc(size)) != NULL) {
        new->size = size;
        new->nwritten = new->nread = 0;
        return new;
    }

    free(new);
    return NULL;
}

// Frees a membuf buffer.
void membuf_free(membuf this)
{
    if (this != NULL) {
        free(this->data);
        free(this);
    }
}
void membuf_freev(void *p)
{
    membuf_free(p);
}

size_t membuf_canread(membuf mb)
{
    assert(mb != NULL);
    assert(mb->nwritten >= mb->nread);
    assert(mb->nwritten - mb->nread <= mb->size);

    return mb->nwritten - mb->nread;
}

void membuf_set_written(membuf mb, size_t cb)
{
    assert(mb->nwritten == 0);
    mb->nwritten = cb;
}

// Return the number of bytes available for writing.
size_t membuf_canwrite(membuf mb)
{
    assert(mb != NULL);

    /* Report full size if next write will reset anyway */
    if (mb->nread == mb->nwritten)
        return mb->size;
    else
        return mb->size - mb->nwritten;
}

/* Empties the content of the buffer.  */
void membuf_reset(membuf mb)
{
    assert(mb != NULL);
    mb->nread = mb->nwritten = 0;
}

status_t membuf_unget(membuf mb)
{
    assert(mb != NULL);

    if (mb->nread > 0) {
        mb->nread--;
        return success;
    }

    return failure;
}

void* membuf_data(membuf mb)
{
    assert(mb != NULL);
    return mb->data;
}

void membuf_set(membuf mb, int c)
{
    assert(mb != NULL);
    memset(mb->data, c, mb->size);
}

size_t membuf_size(membuf mb)
{
    assert(mb != NULL);
    return mb->size;
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

    memcpy(dest, &this->data[this->nread], count);
    this->nread += count;

    assert(this->nread <= this->nwritten);

    // Reset offset counters if all bytes written also have been read
    if (this->nwritten == this->nread)
        this->nwritten = this->nread = 0;

    return count;
}

size_t membuf_write(membuf this, const void *src, size_t count)
{
    size_t navail;

    assert(this != NULL);
    assert(src != NULL);

    navail = membuf_canwrite(this);
    if (count > navail)
        count = navail;

    memcpy(&this->data[this->nwritten], src, count);
    this->nwritten += count;

    return count;
}
#ifdef CHECK_MEMBUF

#include <string.h>

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
        (void)cb;
    }

    // Now loop with odd sizes to see if we miss a byte or two. Size stuff so
    // that we write less than we read, and that size % writes != 0.
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
