/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#define _GNU_SOURCE 1
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <openssl/ssl.h>

#ifdef _XOPEN_SOURCE_EXTENDED
//#include <arpa/inet.h>
#endif

#include <gensocket.h>
#include <tcpsocket.h>

socket_t tcpsocket_create_client_socket(const char *host, int port)
{
    char serv[6];
    struct addrinfo hints = {0}, *res = NULL, *ai;
    socket_t this = NULL;

    assert(host != NULL);

    snprintf(serv, sizeof serv, "%u", (unsigned)port);
    hints.ai_family   = AF_UNSPEC;          // v4/v6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_ADDRCONFIG | AI_NUMERICSERV;

    int rc = getaddrinfo(host, serv, &hints, &res);
    if (rc)
        return NULL;

    for (ai = res; ai; ai = ai->ai_next) {
        this = socket_socket(ai);
        if (this == NULL)
            continue;

        if (connect(socket_get_fd(this), ai->ai_addr, ai->ai_addrlen) == 0)
            break;

        socket_close(this);
        this = NULL;
    }

    freeaddrinfo(res);
    if (this == NULL)
        return NULL;

    if (!gensocket_set_nonblock(socket_get_fd(this))) {
        socket_close(this);
        return NULL;
    }

    return this;
}

