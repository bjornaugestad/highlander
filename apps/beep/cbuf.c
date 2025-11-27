#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cbuf.h"

writebuf writebuf_new(size_t initial_size, bool can_grow)
{
    writebuf new = malloc(sizeof *new);
    if (new == NULL)
        return NULL;

    new->buf = malloc(initial_size);
    if (new->buf == NULL) {
        free(new);
        return NULL;
    }

    new->can_grow = can_grow;
    new->bufsize = initial_size;
    new->nused = 0;
    return new;
}

void writebuf_free(writebuf this)
{
    if (this != NULL) {
        free(this->buf);
        free(this);
    }
}

static inline size_t freespace(const writebuf this)
{
    return this->bufsize - this->nused;
}

static inline status_t makeroom(writebuf this, size_t nrequired)
{
    size_t nfree = freespace(this);
    if (nrequired <= nfree)
        return success;

    // We need more room. Double the buffer's size
    size_t newsize = this->bufsize * 2;
    unsigned char *tmp = realloc(this->buf, newsize);
    if (tmp == NULL)
        return failure;

    this->buf = tmp;
    this->bufsize = newsize;
    return success;
}

static inline void write16(writebuf this, uint16_t val)
{
    this->buf[this->nused++] = (unsigned char)((val & 0xff00u) >> 8u);
    this->buf[this->nused++] = (unsigned char)(val & 0xff);
}
static inline void write32(writebuf this, uint32_t val)
{
    this->buf[this->nused++] = (unsigned char)((val & 0xff000000) >> 24);
    this->buf[this->nused++] = (unsigned char)((val & 0xff0000) >> 16);
    this->buf[this->nused++] = (unsigned char)((val & 0xff00) >> 8);
    this->buf[this->nused++] = (unsigned char)(val & 0xff);
}

// Tag is already written. This fn is here to merge redundant code
static inline void write64(writebuf this, uint64_t val)
{
    this->buf[this->nused++] = (unsigned char)((val & 0xff00000000000000) >> 56);
    this->buf[this->nused++] = (unsigned char)((val & 0xff000000000000) >> 48);
    this->buf[this->nused++] = (unsigned char)((val & 0xff0000000000) >> 40);
    this->buf[this->nused++] = (unsigned char)((val & 0xff00000000) >> 32);
    this->buf[this->nused++] = (unsigned char)((val & 0xff000000) >> 24);
    this->buf[this->nused++] = (unsigned char)((val & 0xff0000) >> 16);
    this->buf[this->nused++] = (unsigned char)((val & 0xff00) >> 8);
    this->buf[this->nused++] = (unsigned char)(val & 0xff);
}

status_t writebuf_int8(writebuf wb, int8_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'c';
    wb->buf[wb->nused++] = (unsigned char)val;
    return success;
}

status_t writebuf_uint8(writebuf wb, uint8_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'C';
    wb->buf[wb->nused++] = val;
    return success;
}

status_t writebuf_int16(writebuf wb, int16_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'h';
    write16(wb, (uint16_t)val);
    return success;
}

status_t writebuf_uint16(writebuf wb, uint16_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'H';
    write16(wb, val);
    return success;
}

status_t writebuf_int32(writebuf wb, int32_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'i';
    write32(wb, (uint32_t)val);
    return success;
}

status_t writebuf_uint32(writebuf wb, uint32_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'I';
    write32(wb, val);
    return success;
}

status_t writebuf_int64(writebuf wb, int64_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    write64(wb, (uint64_t)val);
    return success;
}

status_t writebuf_uint64(writebuf wb, uint64_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'L';
    write64(wb, val);
    return success;
}

status_t writebuf_header(writebuf wb, struct beep_header *h)
{
    if (!makeroom(wb, sizeof *h))
        return failure;

    write16(wb, h->version);
    write16(wb, h->request);
    return success;
}

status_t writebuf_float(writebuf wb, float val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;
    wb->buf[wb->nused++] = 'f';

    char tmp[4];
    memcpy(tmp, &val, 4);
    return writebuf_blob(wb, tmp, 4);
}

status_t writebuf_double(writebuf wb, double val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'd';
    char tmp[8];
    memcpy(tmp, &val, 8);
    return writebuf_blob(wb, tmp, 8);
}

status_t writebuf_datetime(writebuf wb, int64_t val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'D';
    write64(wb, (uint64_t)val);
    return success;
}

status_t writebuf_bool(writebuf wb, bool val)
{
    assert(wb != NULL);

    if (!makeroom(wb, sizeof val + 1))
        return failure;

    wb->buf[wb->nused++] = 'b';
    wb->buf[wb->nused++] = val ? 't' : 'f';
    return success;
}

status_t writebuf_null(writebuf wb)
{
    assert(wb != NULL);

    if (!makeroom(wb, 1))
        return failure;

    wb->buf[wb->nused++] = 'Z';
    return success;
}

status_t writebuf_string(writebuf wb, const char *src)
{
    assert(wb != NULL);
    size_t cb = strlen(src); // We prefix with len(max 4GB uint32)

    if (!makeroom(wb, cb + 4)) // 4 for strlength
        return failure;

    wb->buf[wb->nused++] = 'Q';
    write32(wb, (uint32_t)cb);      // String length, 4 bytes
    memcpy(wb->buf + wb->nused, src, cb);
    wb->nused += cb;
    return success;
}

status_t writebuf_blob(writebuf wb, const void *buf, size_t buflen)
{
    assert(wb != NULL);

    if (!makeroom(wb, buflen + 4))
        return failure;

    wb->buf[wb->nused++] = 'X';
    write32(wb, (uint32_t)buflen);
    memcpy(wb->buf + wb->nused, buf, buflen);
    wb->nused += buflen;
    return success;
}

status_t writebuf_array_start(writebuf wb)
{
    assert(wb != NULL);

    if (!makeroom(wb, 1))
        return failure;

    wb->buf[wb->nused++] = '[';
    return success;
}

status_t writebuf_array_end(writebuf wb)
{
    assert(wb != NULL);

    if (!makeroom(wb, 1))
        return failure;

    wb->buf[wb->nused++] = ']';
    return success;
}

status_t writebuf_object_start(writebuf wb)
{
    assert(wb != NULL);

    if (!makeroom(wb, 1))
        return failure;

    wb->buf[wb->nused++] = '{';
    return success;
}

status_t writebuf_object_end(writebuf wb)
{
    assert(wb != NULL);

    if (!makeroom(wb, 1))
        return failure;

    wb->buf[wb->nused++] = '}';
    return success;
}

readbuf readbuf_new(const unsigned char *buf, size_t size)
{
    assert(buf != NULL);
    assert(size > 0);
    readbuf new = malloc(sizeof *new);
    if (new != NULL) {
        new->buf = buf;
        new->bufsize = size;
        new->nread = 0;
    }

    return new;
}

void readbuf_free(readbuf wb)
{
    free(wb); // We don't manage buffers
}

static inline size_t nunread(readbuf rb)
{
    assert(rb != NULL);
    return rb->bufsize - rb->nread;
}

static inline bool eob(readbuf rb)
{
    return nunread(rb) == 0;
}

static inline int peek(readbuf rb)
{
    assert(rb != NULL);
    assert(!eob(rb));

    return rb->buf[rb->nread];
}

// We expect a certain datatype. This function checks for it.
// If true, file pointer(nread) is moved. You should check for
// 'EOF' before calling this function to get the semantics right.
static inline bool expect(readbuf rb, int c)
{
    assert(rb != NULL);
    if (peek(rb) != c)
        return false;

    rb->nread++;
    return true;
}

static inline void read16(readbuf rb, uint16_t *dest)
{
    assert(rb != NULL);
    assert(dest != NULL);
    assert(nunread(rb) >= sizeof *dest);

    *dest = (uint16_t)(rb->buf[rb->nread++] << 8u);
    *dest |= rb->buf[rb->nread++];
}

static inline void read32(readbuf rb, uint32_t *dest)
{
    assert(rb != NULL);
    assert(dest != NULL);
    assert(nunread(rb) >= sizeof *dest);

    *dest  = (uint32_t)rb->buf[rb->nread++] << 24;
    *dest |= (uint32_t)rb->buf[rb->nread++] << 16;
    *dest |= (uint32_t)rb->buf[rb->nread++] << 8;
    *dest |= (uint32_t)rb->buf[rb->nread++];
}

static inline void read64(readbuf rb, uint64_t *dest)
{
    assert(rb != NULL);
    assert(dest != NULL);
    assert(nunread(rb) >= sizeof *dest);

    *dest  = (uint64_t)rb->buf[rb->nread++] << 56;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 48;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 40;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 32;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 24;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 16;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 8;
    *dest |= rb->buf[rb->nread++];
}

status_t readbuf_int8(readbuf rb, int8_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'c'))
        return failure;

    *val = (int8_t)rb->buf[rb->nread++];
    return success;
}

status_t readbuf_uint8(readbuf rb, uint8_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'C'))
        return failure;

    *val = rb->buf[rb->nread++];
    return success;
}

status_t readbuf_int16(readbuf rb, int16_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'h'))
        return failure;

    read16(rb, (uint16_t*)val);
    return success;
}

status_t readbuf_uint16(readbuf rb, uint16_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'H'))
        return failure;

    read16(rb, val);
    return success;
}

status_t readbuf_int32(readbuf rb, int32_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'i'))
        return failure;

    read32(rb, (uint32_t*)val);
    return success;
}

status_t readbuf_uint32(readbuf rb, uint32_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'I'))
        return failure;

    read32(rb, val);
    return success;
}

status_t readbuf_int64(readbuf rb, int64_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'l'))
        return failure;

    read64(rb, (uint64_t*)val);
    return success;
}

status_t readbuf_uint64(readbuf rb, uint64_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'L'))
        return failure;

    read64(rb, val);
    return success;

}

status_t readbuf_float(readbuf rb, float *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);
    if (!expect(rb, 'f'))
        return failure;

    read32(rb, (uint32_t*)val);
    return success;

}

status_t readbuf_double(readbuf rb, double *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'd'))
        return failure;

    read64(rb, (uint64_t*)val);
    return success;
}

status_t readbuf_datetime(readbuf rb, int64_t *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'D'))
        return failure;

    read64(rb, (uint64_t*)val);
    return success;
}

status_t readbuf_bool(readbuf rb, bool  *val)
{
    assert(rb != NULL);
    assert(nunread(rb) > sizeof *val);

    if (!expect(rb, 'b'))
        return failure;

    *val = rb->buf[rb->nread++] == 't' ? true : false;
    return success;
}

status_t readbuf_null(readbuf rb)
{
    assert(rb != NULL);
    return expect(rb, 'Z') ? success : failure;
}

status_t readbuf_array_start(readbuf rb)
{
    assert(rb != NULL);
    return expect(rb, '[') ? success : failure;
}

status_t readbuf_array_end(readbuf rb)
{
    assert(rb != NULL);

    return expect(rb, ']') ? success : failure;
}

status_t readbuf_object_start(readbuf rb)
{
    assert(rb != NULL);
    return expect(rb, '{') ? success : failure;

}

status_t readbuf_object_end(readbuf rb)
{
    assert(rb != NULL);
    return expect(rb, '}') ? success : failure;

}

status_t readbuf_string(readbuf rb, char *dest, size_t destsize)
{
    assert(rb != NULL);
    assert(dest != NULL);

    if (!expect(rb, 'Q'))
        return failure;

    uint32_t len;
    read32(rb, &len);
    if (len >= destsize) // need room for nil
        return failure;

    memcpy(dest, rb->buf + rb->nread, len);
    rb->nread += len;
    dest[len] = '\0';
    return success;
}

status_t readbuf_blob(readbuf rb, void *dest, size_t destsize)
{
    assert(rb != NULL);
    if (!expect(rb, 'X'))
        return failure;

    uint32_t len;
    read32(rb, &len);
    if (len > destsize)
        return failure;

    memcpy(dest, rb->buf + rb->nread, len);
    rb->nread += len;
    return success;
}


#ifdef CBUF_CHECK
#include "beep_db.h"

static void user_write(writebuf wb, User u)
{
    uint64_t id = user_id(u);
    const char *name = user_name(u), *nick = user_nick(u), *email = user_email(u);

    if (!writebuf_uint64(wb, id)
    ||  !writebuf_string(wb, name)
    ||  !writebuf_string(wb, nick)
    ||  !writebuf_string(wb, email))
        die("Cant marshal");
}

static void user_read(readbuf rb, User u)
{
    if (!readbuf_uint64(rb, &u->id)
    ||  !readbuf_string(rb, u->name, sizeof u->name)
    ||  !readbuf_string(rb, u->nick, sizeof u->nick)
    ||  !readbuf_string(rb, u->email, sizeof u->email))
        die("Cant unmarshal");
}

int main(void)
{
    writebuf wb = writebuf_new(1024*1024, false);
    if (wb == NULL)
        die("_new");


    // Create two user objects. Fill one, then serialise it to writebuf.
    // Then unmarshal data from a readbuf into the second object. Then compare.
    //
    User u1 = user_new(), u2 = user_new();
    user_set_id(u1, 1);
    user_set_name(u1, "Hello, world");
    user_set_nick(u1, "my nickname");
    user_set_email(u1, "foo@bar.com");

    user_write(wb, u1);

    // Make a readbuf.
    readbuf rb = readbuf_new(writebuf_buf(wb), writebuf_len(wb));
    if (rb == NULL)
        die("_new rb");

    user_read(rb, u2);

    readbuf_free(rb);
    writebuf_free(wb);

    // So, are u1 and u2 equal? Let's find out.
    if (user_id(u1) != user_id(u2))
        die("UID mismatch");

    if (strcmp(user_name(u1), user_name(u2)) != 0)
        die("Name mismatch");

    if (strcmp(user_nick(u1), user_nick(u2)) != 0)
        die("nick mismatch");

    if (strcmp(user_email(u1), user_email(u2)) != 0)
        die("email mismatch");

    user_free(u1);
    user_free(u2);

    return 0;
}

#endif

