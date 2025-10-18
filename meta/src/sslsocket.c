/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#define _GNU_SOURCE 1
#include <unistd.h>
#include <stdio.h>
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
#include <openssl/err.h>

#include <gensocket.h>
#include <sslsocket.h>





// 20251008: New version, loosely based on a client program chatgpt wrote
// to test the echoserver when nc failed to perform properly. We throw away
// all the old code which used BIOs a lot. Fuck BIO! Worst concept ever.
//
// So what do we do here? We open a socket connection to a server. 
// IOW, we create a 'socket', we 'connect' and we create an SSL object too.
//
// Assumptions: 
// - An SSL context already exist. (SSL_CTX_new() et al)
// - We don't manage signals here.
// - We're IP v4/v6 agnostic
//

sslsocket sslsocket_create_client_socket(void *context,
    const char *host, int port)
{
// It's a PITA to get rid of BIO, so we disable shit for a while boa@20251008
(void)context; (void)host;(void)port;

    assert(context != NULL);
    assert(host != NULL);
    assert(strlen(host) > 0);
    assert(port > 0);

    return NULL;

}


