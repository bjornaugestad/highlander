/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "internals.h"
#include <highlander.h>

typedef struct http_status_struct {
    int code;
    const char *text;
 } http_status;

static http_status m_http_status10[] = {
     { 200, "HTTP/1.0 200 OK\r\n" },
     { 201, "HTTP/1.0 201 Created\r\n" },
     { 202, "HTTP/1.0 202 Accepted\r\n" },
     { 204, "HTTP/1.0 204 No Content\r\n" },
     { 205, "HTTP/1.0 205 Reset Content\r\n" },
     { 206, "HTTP/1.0 206 Partial Content\r\n" },
     { 301, "HTTP/1.0 301 Moved Permanently\r\n" },
     { 302, "HTTP/1.0 302 Found\r\n" },
     { 304, "HTTP/1.0 304 Not Modified\r\n" },
     { 400, "HTTP/1.0 400 Bad Request\r\n" },
     { 401, "HTTP/1.0 401 Unauthorized\r\n" },
     { 403, "HTTP/1.0 403 Forbidden\r\n" },
     { 404, "HTTP/1.0 404 Not Found\r\n" },
     { 500, "HTTP/1.0 500 Internal Server Error\r\n" },
     { 501, "HTTP/1.0 501 Not Implemented\r\n" },
     { 502, "HTTP/1.0 502 Bad Gateway\r\n" },
     { 503, "HTTP/1.0 503 Service Unavailable\r\n" },
 };

static http_status m_http_status11[] = {
    { 100, "HTTP/1.1 100 Continue\r\n" },
    { 101, "HTTP/1.1 101 Switching Protocols\r\n" },
    { 200, "HTTP/1.1 200 OK\r\n" },
    { 201, "HTTP/1.1 201 Created\r\n" },
    { 202, "HTTP/1.1 202 Accepted\r\n" },
    { 203, "HTTP/1.1 203 Non-Authorative Information\r\n" },
    { 204, "HTTP/1.1 204 No Content\r\n" },
    { 205, "HTTP/1.1 205 Reset Content\r\n" },
    { 206, "HTTP/1.1 206 Partial Content\r\n" },
    { 300, "HTTP/1.1 300 Multiple Choices\r\n" },
    { 301, "HTTP/1.1 301 Moved Permanently\r\n" },
    { 302, "HTTP/1.1 302 Found\r\n" },
    { 303, "HTTP/1.1 303 See Other\r\n" },
    { 304, "HTTP/1.1 304 Not Modified\r\n" },
    { 305, "HTTP/1.1 305 Use Proxy\r\n" },
    { 307, "HTTP/1.1 307 Temporary Redirect\r\n" },
    { 400, "HTTP/1.1 400 Bad Request\r\n" },
    { 401, "HTTP/1.1 401 Unauthorized\r\n" },
    { 402, "HTTP/1.1 402 Payment Required\r\n" },
    { 403, "HTTP/1.1 403 Forbidden\r\n" },
    { 404, "HTTP/1.1 404 Not Found\r\n" },
    { 405, "HTTP/1.1 405 Method Not Allowed\r\n" },
    { 406, "HTTP/1.1 406 Not Acceptable\r\n" },
    { 407, "HTTP/1.1 407 Proxy Authentication Required\r\n" },
    { 408, "HTTP/1.1 408 Request Time-out\r\n" },
    { 409, "HTTP/1.1 409 Conflict\r\n" },
    { 410, "HTTP/1.1 410 Gone\r\n" },
    { 411, "HTTP/1.1 411 Length Required\r\n" },
    { 412, "HTTP/1.1 412 Precondition Failed\r\n" },
    { 413, "HTTP/1.1 413 Request Entity Too Large\r\n" },
    { 414, "HTTP/1.1 414 Request-URI Too Large\r\n" },
    { 415, "HTTP/1.1 415 Unsupported Media Type\r\n" },
    { 416, "HTTP/1.1 416 Requested range not satisfiable\r\n" },
    { 417, "HTTP/1.1 417 Expectation Failed\r\n" },
    { 500, "HTTP/1.1 500 Internal Server Error\r\n" },
    { 501, "HTTP/1.1 501 Not Implemented\r\n" },
    { 502, "HTTP/1.1 502 Bad Gateway\r\n" },
    { 503, "HTTP/1.1 503 Service Unavailable\r\n" },
    { 504, "HTTP/1.1 504 Gateway time-out\r\n" },
    { 505, "HTTP/1.1 505 HTTP Version not supported\r\n" },
};

status_t send_status_code(connection conn, int status_code, int version)
{
    size_t i, cb, n;
    http_status* pstatus;

    assert(conn != NULL);

    if (version == VERSION_10) {
        pstatus = m_http_status10;
        n = sizeof m_http_status10 / sizeof *m_http_status10;
    }
    else {
        pstatus = m_http_status11;
        n = sizeof m_http_status11 / sizeof *m_http_status11;
    }

    for (i = 0; i < n; i++) {
        if (pstatus[i].code == status_code) {
            cb = strlen(pstatus[i].text);
            return connection_write(conn, pstatus[i].text, cb);
        }
    }

    /* Unsupported status code? Pretty internal error, isn't it?
     * We just die as we obviously have a major internal error */
    fprintf(stderr, "Weird status code: %d (hex %x)\n", status_code, status_code);
    return 0;
    abort();
}
