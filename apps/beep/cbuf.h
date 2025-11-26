#ifndef BEEP_CBUF_H
#define BEEP_CBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <meta_common.h>

// So for the beep protocol we need a header. What goes in the header?
// Who knows, but we need a request-id, a version, and the payload.
// Keep it simple for as long as possible. 
struct header {
    uint16_t version;
    uint16_t request; // As in what we want to do. 
    uint64_t payload_len;
    unsigned char *payload;
};

// We need a character buffer to serialize data to and from.
// We need access functions, setters and getters, and we need
// memory management. That's what the cbuf is for.
//
// A cbuf grows and grows if that's what we want. It's opened
// in a read mode or write mode(append). The read mode has 
// a cursor. We assign buffers to read-mode objects. We allocate
// buffers in write mode so we can realloc() on demand. 

typedef struct writebuf_tag {
    unsigned char *buf;
    size_t bufsize;
    size_t nused;  // nused in write mode.
    bool can_grow;

} *writebuf;

writebuf writebuf_new(size_t initial_size, bool can_grow);
void writebuf_free(writebuf this);
static inline const unsigned char *writebuf_buf(writebuf wb) {return wb->buf;}
static inline size_t writebuf_len(writebuf wb) {return wb->nused;}

status_t writebuf_int8(writebuf wb, int8_t val) __attribute__((warn_unused_result));
status_t writebuf_uint8(writebuf wb, uint8_t val) __attribute__((warn_unused_result));
status_t writebuf_int16(writebuf wb, int16_t val) __attribute__((warn_unused_result));
status_t writebuf_uint16(writebuf wb, uint16_t val) __attribute__((warn_unused_result));
status_t writebuf_int32(writebuf wb, int32_t val) __attribute__((warn_unused_result));
status_t writebuf_uint32(writebuf wb, uint32_t val) __attribute__((warn_unused_result));
status_t writebuf_int64(writebuf wb, int64_t val) __attribute__((warn_unused_result));
status_t writebuf_uint64(writebuf wb, uint64_t val) __attribute__((warn_unused_result));
status_t writebuf_float(writebuf wb, float val) __attribute__((warn_unused_result));
status_t writebuf_double(writebuf wb, double val) __attribute__((warn_unused_result));
status_t writebuf_datetime(writebuf wb, int64_t val) __attribute__((warn_unused_result));
status_t writebuf_bool(writebuf wb, bool val) __attribute__((warn_unused_result));
status_t writebuf_null(writebuf wb) __attribute__((warn_unused_result));
status_t writebuf_string(writebuf wb, const char *src) __attribute__((warn_unused_result));
status_t writebuf_blob(writebuf wb, const void *buf, size_t buflen) __attribute__((warn_unused_result));

status_t writebuf_array_start(writebuf wb) __attribute__((warn_unused_result));
status_t writebuf_array_end(writebuf wb) __attribute__((warn_unused_result));
status_t writebuf_object_start(writebuf wb) __attribute__((warn_unused_result));
status_t writebuf_object_end(writebuf wb) __attribute__((warn_unused_result));

typedef struct readbuf_tag {
    const unsigned char *buf;
    size_t bufsize;
    size_t nread;

} *readbuf;

readbuf readbuf_new(const unsigned char *buf, size_t size);
void readbuf_free(readbuf this);

status_t readbuf_int8(readbuf wb, int8_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint8(readbuf wb, uint8_t *val) __attribute__((warn_unused_result));
status_t readbuf_int16(readbuf wb, int16_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint16(readbuf wb, uint16_t *val) __attribute__((warn_unused_result));
status_t readbuf_int32(readbuf wb, int32_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint32(readbuf wb, uint32_t *val) __attribute__((warn_unused_result));
status_t readbuf_int64(readbuf wb, int64_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint64(readbuf wb, uint64_t *val) __attribute__((warn_unused_result));
status_t readbuf_float(readbuf wb, float *val) __attribute__((warn_unused_result));
status_t readbuf_double(readbuf wb, double *val) __attribute__((warn_unused_result));
status_t readbuf_datetime(readbuf wb, int64_t *val) __attribute__((warn_unused_result));
status_t readbuf_bool(readbuf wb, bool  *val) __attribute__((warn_unused_result));
status_t readbuf_null(readbuf wb) __attribute__((warn_unused_result));

status_t readbuf_array_start(readbuf wb) __attribute__((warn_unused_result));
status_t readbuf_array_end(readbuf wb) __attribute__((warn_unused_result));
status_t readbuf_object_start(readbuf wb) __attribute__((warn_unused_result));
status_t readbuf_object_end(readbuf wb) __attribute__((warn_unused_result));
status_t readbuf_string(readbuf wb, char *dest, size_t destsize) __attribute__((warn_unused_result));
status_t readbuf_blob(readbuf wb, void *dest, size_t destsize) __attribute__((warn_unused_result));

// TODO: Add expect-functions for parser


#endif

