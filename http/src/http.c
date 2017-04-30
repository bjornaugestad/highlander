#include <errno.h>
#include <stdio.h>

#include <highlander.h>
#include <internals.h>
/*
 * Reads one line, terminated by \r\n, off the socket.
 * The \r\n is removed, if present.
 *
 * NOTE: According to RFC 2616, §4.2, Header Fields may extend over
 * many lines. I qoute:
 *	Header fields can be extended over multiple lines by preceding each extra
 *	line with at least one SP or HT. Applications ought to follow "common form",
 *	where one is known or indicated, when generating HTTP constructs, since
 *	there might exist some implementations that fail to accept anything
 *	beyond the common forms.
 *	End quote:
 * This means that
 *	a) One header field name will never occur twice.
 *	b) A Field value may span multiple lines.
 * Which means that we must read-ahead one byte after the \r\n and look
 * for either SP or HT.
    I am not sure if we want to support wrapped lines,
    as it may cause a lot of waiting in poll(). Imagine
    that the client sends
        GET / HTTP/1.0
        Connection: Keep-Alive
    We read the Get and the Connection lines. Then what?
    Do we then want to look for another char after the last line?
    The client didn't send one, so we end up in poll() and
    wait for a timeout :-(
 */
status_t read_line(connection conn, char* buf, size_t cchMax, error e)
{
    int c;
    size_t i = 0;

    while (i < cchMax) {
        if (!connection_getc(conn, &c))
            return set_tcpip_error(e, errno);

        if (c == '\r') {
            /* We got a \r. Look for \n */
            buf[i] = '\0';

            if (!connection_getc(conn, &c))
                return set_tcpip_error(e, errno);

            if (c != '\n')
                return set_http_error(e, HTTP_400_BAD_REQUEST);

            return success;
        }

        buf[i++] = c;
    }

    /* The buffer provided was too small. */
    return set_app_error(e, ENOSPC);
}
