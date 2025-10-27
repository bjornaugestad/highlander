// Just gather misc internal stuff here
//
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <connection.h>

#include <internals.h>


status_t http_send_date(connection conn, const char *name, time_t value)
{
    char date[100];
    size_t cb;
    struct tm t, *ptm = &t;

    assert(name != NULL);

    cb = strlen(name);
    if (connection_write(conn, name, cb)) {
        gmtime_r(&value, ptm);
        cb = strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT\r\n", ptm);
        return connection_write(conn, date, cb);
    }

    return failure;
}

status_t http_send_string(connection conn, const char *s)
{
    assert(conn != NULL);
    assert(s != NULL);

    size_t cb = strlen(s);
    return connection_write(conn, s, cb);
}

status_t http_send_ulong(connection conn, const char *name, unsigned long value)
{
    assert(conn != NULL);
    assert(name != NULL);

    char val[1000];
    int cb = snprintf(val, sizeof val, "%s%lu", name, value);
    if (cb < 0 || (size_t)cb >= sizeof val)
        return failure;

    return connection_write(conn, val, (size_t)cb);
}

status_t http_send_int(connection conn, const char *name, int value)
{
    assert(conn != NULL);
    assert(name != NULL);

    char val[1000];
    int cb = snprintf(val, sizeof val, "%s%d", name, value);
    if (cb < 0 || (size_t)cb >= sizeof val)
        return failure;

    return connection_write(conn, val, (size_t)cb);
}

status_t http_send_unsigned_int(connection conn, const char *name, unsigned int value)
{
    assert(conn != NULL);
    assert(name != NULL);

    char val[1000];
    int cb = snprintf(val, sizeof val, "%s%u", name, value);
    if (cb < 0 || (size_t)cb >= sizeof val)
        return failure;

    return connection_write(conn, val, (size_t)cb);
}

status_t http_send_field(connection conn, const char *name, cstring value)
{
    assert(conn != NULL);
    assert(name != NULL);
    assert(value != NULL);

    size_t cb = strlen(name);
    if (!connection_write(conn, name, cb))
        return failure;

    cb = cstring_length(value);
    if (!connection_write(conn, c_str(value), cb))
        return failure;

    return connection_write(conn, "\r\n", 2);
}

