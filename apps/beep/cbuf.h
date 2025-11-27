#ifndef BEEP_CBUF_H
#define BEEP_CBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <meta_common.h>
#include <connection.h>

#define BEEP_VERSION 0x01
struct beep_header {
    uint16_t version;
    uint16_t request;
};

status_t writebuf_header(connection conn, struct beep_header *val) __attribute__((warn_unused_result));
status_t writebuf_int8(connection conn, int8_t val) __attribute__((warn_unused_result));
status_t writebuf_uint8(connection conn, uint8_t val) __attribute__((warn_unused_result));
status_t writebuf_int16(connection conn, int16_t val) __attribute__((warn_unused_result));
status_t writebuf_uint16(connection conn, uint16_t val) __attribute__((warn_unused_result));
status_t writebuf_int32(connection conn, int32_t val) __attribute__((warn_unused_result));
status_t writebuf_uint32(connection conn, uint32_t val) __attribute__((warn_unused_result));
status_t writebuf_int64(connection conn, int64_t val) __attribute__((warn_unused_result));
status_t writebuf_uint64(connection conn, uint64_t val) __attribute__((warn_unused_result));
status_t writebuf_float(connection conn, float val) __attribute__((warn_unused_result));
status_t writebuf_double(connection conn, double val) __attribute__((warn_unused_result));
status_t writebuf_datetime(connection conn, int64_t val) __attribute__((warn_unused_result));
status_t writebuf_bool(connection conn, bool val) __attribute__((warn_unused_result));
status_t writebuf_null(connection conn) __attribute__((warn_unused_result));
status_t writebuf_string(connection conn, const char *src) __attribute__((warn_unused_result));
status_t writebuf_blob(connection conn, const void *buf, size_t buflen) __attribute__((warn_unused_result));

status_t writebuf_array_start(connection conn) __attribute__((warn_unused_result));
status_t writebuf_array_end(connection conn) __attribute__((warn_unused_result));
status_t writebuf_object_start(connection conn) __attribute__((warn_unused_result));
status_t writebuf_object_end(connection conn) __attribute__((warn_unused_result));

status_t readbuf_int8(connection conn, int8_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint8(connection conn, uint8_t *val) __attribute__((warn_unused_result));
status_t readbuf_int16(connection conn, int16_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint16(connection conn, uint16_t *val) __attribute__((warn_unused_result));
status_t readbuf_int32(connection conn, int32_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint32(connection conn, uint32_t *val) __attribute__((warn_unused_result));
status_t readbuf_int64(connection conn, int64_t *val) __attribute__((warn_unused_result));
status_t readbuf_uint64(connection conn, uint64_t *val) __attribute__((warn_unused_result));
status_t readbuf_float(connection conn, float *val) __attribute__((warn_unused_result));
status_t readbuf_double(connection conn, double *val) __attribute__((warn_unused_result));
status_t readbuf_datetime(connection conn, int64_t *val) __attribute__((warn_unused_result));
status_t readbuf_bool(connection conn, bool  *val) __attribute__((warn_unused_result));
status_t readbuf_null(connection conn) __attribute__((warn_unused_result));

status_t readbuf_array_start(connection conn) __attribute__((warn_unused_result));
status_t readbuf_array_end(connection conn) __attribute__((warn_unused_result));
status_t readbuf_object_start(connection conn) __attribute__((warn_unused_result));
status_t readbuf_object_end(connection conn) __attribute__((warn_unused_result));
status_t readbuf_string(connection conn, char *dest, size_t destsize) __attribute__((warn_unused_result));
status_t readbuf_blob(connection conn, void *dest, size_t destsize) __attribute__((warn_unused_result));


#endif

