#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cbuf.h"

static inline status_t write16(connection conn, uint16_t val)
{
    unsigned char buf[2];
    buf[0] = (unsigned char)((val & 0xff00u) >> 8u);
    buf[1] = (unsigned char)(val  & 0xff);
    return connection_write(conn, buf, sizeof buf);
}
static inline status_t write32(connection conn, uint32_t val)
{
    unsigned char buf[4];
    buf[0] = (unsigned char)((val & 0xff000000) >> 24);
    buf[1] = (unsigned char)((val & 0xff0000) >> 16);
    buf[2] = (unsigned char)((val & 0xff00) >> 8);
    buf[3] = (unsigned char)(val & 0xff);
    return connection_write(conn, buf, sizeof buf);
}

// Tag is already written. This fn is here to merge redundant code
static inline status_t write64(connection conn, uint64_t val)
{
    unsigned char buf[8];
    buf[0] = (unsigned char)((val & 0xff00000000000000) >> 56);
    buf[1] = (unsigned char)((val & 0xff000000000000) >> 48);
    buf[2] = (unsigned char)((val & 0xff0000000000) >> 40);
    buf[3] = (unsigned char)((val & 0xff00000000) >> 32);
    buf[4] = (unsigned char)((val & 0xff000000) >> 24);
    buf[5] = (unsigned char)((val & 0xff0000) >> 16);
    buf[6] = (unsigned char)((val & 0xff00) >> 8);
    buf[7] = (unsigned char)(val & 0xff);
    return connection_write(conn, buf, sizeof buf);
}

status_t writebuf_int8(connection conn, int8_t val)
{
    assert(conn != NULL);

    return connection_putc(conn, 'c') && connection_putc(conn, val) ? success : failure;
}

status_t writebuf_uint8(connection conn, uint8_t val)
{
    assert(conn != NULL);
    return connection_putc(conn, 'C') && connection_putc(conn, val) ? success : failure;
}

status_t writebuf_int16(connection conn, int16_t val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'h'))
        return failure;

    return write16(conn, (uint16_t)val);
}

status_t writebuf_uint16(connection conn, uint16_t val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'H'))
        return failure;
    return write16(conn, val);
}

status_t writebuf_int32(connection conn, int32_t val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'i'))
        return failure;

    return write32(conn, (uint32_t)val);
}

status_t writebuf_uint32(connection conn, uint32_t val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'I'))
        return failure;

    return write32(conn, (uint32_t)val);
}

status_t writebuf_int64(connection conn, int64_t val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'l'))
        return failure;

    return write64(conn, (uint64_t)val);
}

status_t writebuf_uint64(connection conn, uint64_t val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'L'))
        return failure;

    return write64(conn, (uint64_t)val);
}

status_t writebuf_header(connection conn, struct beep_header *h)
{
    write16(conn, h->version);
    write16(conn, h->request);
    return success;
}

status_t writebuf_float(connection conn, float val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'f'))
        return failure;

    char tmp[4];
    memcpy(tmp, &val, 4);
    return connection_write(conn, tmp, 4);
}

status_t writebuf_double(connection conn, double val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'd'))
        return failure;

    char tmp[8];
    memcpy(tmp, &val, 8);
    return connection_write(conn, tmp, 8);
}

status_t writebuf_datetime(connection conn, int64_t val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'D'))
        return failure;

    return write64(conn, (uint64_t)val);
}

status_t writebuf_bool(connection conn, bool val)
{
    assert(conn != NULL);

    if (!connection_putc(conn, 'b'))
        return failure;

    return connection_putc(conn, val ? 't' : 'f');
}

status_t writebuf_null(connection conn)
{
    assert(conn != NULL);
    return connection_putc(conn, 'Z');
}

status_t writebuf_string(connection conn, const char *src)
{
    assert(conn != NULL);
    size_t cb = strlen(src); // We prefix with len(max 4GB uint32)

    if (!connection_putc(conn, 'Q'))
        return failure;

    // String length, 4 bytes
    if (!write32(conn, (uint32_t)cb))
        return failure;

    return connection_write(conn, src, cb);
}

status_t writebuf_blob(connection conn, const void *buf, size_t buflen)
{
    assert(conn != NULL);
    if (!connection_putc(conn, 'X'))
        return failure;

    if (!write32(conn, (uint32_t)buflen))
        return failure;

    return connection_write(conn, buf, buflen);
}

status_t writebuf_array_start(connection conn)
{
    assert(conn != NULL);

    return connection_putc(conn, '[');
}

status_t writebuf_array_end(connection conn)
{
    assert(conn != NULL);

    return connection_putc(conn, ']');
}

status_t writebuf_object_start(connection conn)
{
    assert(conn != NULL);

    return connection_putc(conn, '{');
}

status_t writebuf_object_end(connection conn)
{
    assert(conn != NULL);

    return connection_putc(conn, '}');
}

// We expect a certain datatype. This function checks for it.
// If true, file pointer(nread) is moved. You should check for
// 'EOF' before calling this function to get the semantics right.
static inline bool expect(connection conn, int val)
{
    assert(conn != NULL);

    char c;
    if (!connection_getc(conn, &c))
        return false;

    if (c == val)
        return true;

    if (!connection_ungetc(conn, c))
        return false;

    return false;
}

#if 0
static inline void read32(connection conn, uint32_t *dest)
{
    assert(conn != NULL);
    assert(dest != NULL);

    *dest  = (uint32_t)rb->buf[rb->nread++] << 24;
    *dest |= (uint32_t)rb->buf[rb->nread++] << 16;
    *dest |= (uint32_t)rb->buf[rb->nread++] << 8;
    *dest |= (uint32_t)rb->buf[rb->nread++];
}

static inline void read64(connection conn, uint64_t *dest)
{
    assert(conn != NULL);
    assert(dest != NULL);

    *dest  = (uint64_t)rb->buf[rb->nread++] << 56;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 48;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 40;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 32;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 24;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 16;
    *dest |= (uint64_t)rb->buf[rb->nread++] << 8;
    *dest |= rb->buf[rb->nread++];
}

#endif

status_t readbuf_int8(connection conn, int8_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'c'))
        return failure;

    return connection_getc(conn, (char *)val);
}

status_t readbuf_uint8(connection conn, uint8_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'C'))
        return failure;

    return connection_getc(conn, (char *)val);
}

status_t readbuf_int16(connection conn, int16_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'h'))
        return failure;

    return connection_read_u16(conn, (uint16_t*)val);
}

status_t readbuf_uint16(connection conn, uint16_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'H'))
        return failure;

    return connection_read_u16(conn, val);
}

status_t readbuf_int32(connection conn, int32_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'i'))
        return failure;

    return connection_read_u32(conn, (uint32_t*)val);
}

status_t readbuf_uint32(connection conn, uint32_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'I'))
        return failure;

    return connection_read_u32(conn, (uint32_t*)val);
}

status_t readbuf_int64(connection conn, int64_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'l'))
        return failure;

    return connection_read_u64(conn, (uint64_t*)val);
}

status_t readbuf_uint64(connection conn, uint64_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'L'))
        return failure;

    return connection_read_u64(conn, (uint64_t*)val);
}

status_t readbuf_float(connection conn, float *val)
{
    assert(conn != NULL);
    if (!expect(conn, 'f'))
        return failure;

    ssize_t nread = connection_read(conn, val, sizeof *val);
    if (nread <= 0 || nread != (ssize_t)sizeof *val)
        return failure;
    
    return success;
}

status_t readbuf_double(connection conn, double *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'd'))
        return failure;

    ssize_t nread = connection_read(conn, val, sizeof *val);
    if (nread <= 0 || nread != (ssize_t)sizeof *val)
        return failure;
    
    return success;
}

status_t readbuf_datetime(connection conn, int64_t *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'D'))
        return failure;

    return connection_read_u64(conn, (uint64_t*)val);
}

status_t readbuf_bool(connection conn, bool *val)
{
    assert(conn != NULL);

    if (!expect(conn, 'b'))
        return failure;

    char c;
    if (!connection_getc(conn, &c))
        return failure;

    if (c == 't')
        *val = true;
    else if (c == 'f')
        *val = false;
    else
        return failure;

    return success;
}

status_t readbuf_null(connection conn)
{
    assert(conn != NULL);
    return expect(conn, 'Z') ? success : failure;
}

status_t readbuf_array_start(connection conn)
{
    assert(conn != NULL);
    return expect(conn, '[') ? success : failure;
}

status_t readbuf_array_end(connection conn)
{
    assert(conn != NULL);

    return expect(conn, ']') ? success : failure;
}

status_t readbuf_object_start(connection conn)
{
    assert(conn != NULL);
    return expect(conn, '{') ? success : failure;

}

status_t readbuf_object_end(connection conn)
{
    assert(conn != NULL);
    return expect(conn, '}') ? success : failure;

}

status_t readbuf_string(connection conn, char *dest, size_t destsize)
{
    assert(conn != NULL);
    assert(dest != NULL);

    if (!expect(conn, 'Q'))
        return failure;

    uint32_t len;
    if (!connection_read_u32(conn, &len))
        return failure;

    if (len >= destsize) // need room for nil
        return failure;

    ssize_t nread = connection_read(conn, dest, len);
    if (nread < 0 || nread != len)
        return failure;

    dest[len] = '\0';
    return success;
}

status_t readbuf_blob(connection conn, void *dest, size_t destsize)
{
    assert(conn != NULL);
    if (!expect(conn, 'X'))
        return failure;

    uint32_t len;
    if (!connection_read_u32(conn, &len))
        return failure;

    if (len > destsize)
        return failure;

    ssize_t nread = connection_read(conn, dest, len);
    if (nread != len)
        return failure;

    return success;
}


#ifdef CBUF_CHECK
#include "beep_db.h"

static void user_write(connection conn, User u)
{
    uint64_t id = user_id(u);
    const char *name = user_name(u), *nick = user_nick(u), *email = user_email(u);

    if (!writebuf_uint64(wb, id)
    ||  !writebuf_string(wb, name)
    ||  !writebuf_string(wb, nick)
    ||  !writebuf_string(wb, email))
        die("Cant marshal");
}

static void user_read(connection conn, User u)
{
    if (!readbuf_uint64(rb, &u->id)
    ||  !readbuf_string(rb, u->name, sizeof u->name)
    ||  !readbuf_string(rb, u->nick, sizeof u->nick)
    ||  !readbuf_string(rb, u->email, sizeof u->email))
        die("Cant unmarshal");
}

int main(void)
{
    connection conn = writebuf_new(1024*1024, false);
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
    connection conn = readbuf_new(writebuf_buf(wb), writebuf_len(wb));
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

