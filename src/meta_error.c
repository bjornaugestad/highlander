/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#include <meta_common.h>
#include <meta_error.h>
#include <meta_misc.h>

enum meta_error_group {
    meg_unknown,
    meg_tcpip,		/* network related error */
    meg_protocol,	/* Protocol, e.g. HTTP, syntax/semantic error */
    meg_app,		/* Some application error, unable to handle */
    meg_os,			/* Some call to the OS failed */
    meg_db,			/* some database error */
    meg_other		/* Other errors */
};

/**
 * Implementation of the meta_error ADT.
 */
struct meta_error_tag {
    enum meta_error_group group;
    int code;
    char message[META_ERROR_MESSAGE_MAX + 1];
};


meta_error meta_error_new(void)
{
    meta_error e;

    e = calloc(1, sizeof *e);
    return e;
}

void meta_error_free(meta_error e)
{
    mem_free(e);
}

int set_tcpip_error(meta_error e, int val)
{
    assert(e != NULL);

    e->group = meg_tcpip;
    e->code = val;

    return 0;
}

int set_http_error(meta_error e, int val)
{
    if (e != NULL) {
        e->group = meg_protocol;
        e->code = val;
    }

    return 0;
}

int set_app_error(meta_error e, int val)
{
    if (e != NULL) {
        e->group = meg_app;
        e->code = val;
    }

    return 0;
}

int set_os_error(meta_error e, int val)
{
    if (e != NULL) {
        e->group = meg_os;
        e->code = val;
    }

    return 0;
}

int set_db_error(meta_error e, int val)
{
    if (e != NULL) {
        e->group = meg_db;
        e->code = val;
    }

    return 0;
}

int set_other_error(meta_error e, int val)
{
    if (e != NULL) {
        e->group = meg_other;
        e->code = val;
    }

    return 0;
}

int is_tcpip_error(meta_error e)
{
    assert(e != NULL);
    return e->group == meg_tcpip;
}

int is_db_error(meta_error e)
{
    assert(e != NULL);
    return e->group == meg_db;
}

int is_protocol_error(meta_error e)
{
    assert(e != NULL);
    return e->group == meg_protocol;
}

int is_app_error(meta_error e)
{
    assert(e != NULL);
    return e->group == meg_app;
}

int is_os_error(meta_error e)
{
    assert(e != NULL);
    return e->group == meg_os;
}

int is_other_error(meta_error e)
{
    assert(e != NULL);
    return e->group == meg_other;
}

int get_error_code(meta_error e)
{
    assert(e != NULL);
    return e->code;
}


void set_error_message(meta_error e, const char *msg)
{
    if (e != NULL) {
        strncpy(e->message, msg, META_ERROR_MESSAGE_MAX);
        e->message[META_ERROR_MESSAGE_MAX] = '\0';
    }
}

int has_error_message(meta_error e)
{
    assert(e != NULL);
    return strlen(e->message) > 0;
}

const char *get_error_message(meta_error e)
{
    assert(e != NULL);
    return e->message;

}

void die_with_error(meta_error e, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);

    if (is_tcpip_error(e))
        syslog(LOG_ERR, "A tcp/ip error has occured");
    else if(is_protocol_error(e))
        syslog(LOG_ERR, "A protocol error has occured");
    else if(is_app_error(e))
        syslog(LOG_ERR, "A application error has occured");
    else if(is_os_error(e))
        syslog(LOG_ERR, "A os error has occured");
    else if(is_db_error(e))
        syslog(LOG_ERR, "A database error has occured");
    else if(is_other_error(e))
        syslog(LOG_ERR, "An unknown error has occured");

    if (has_error_message(e))
        syslog(LOG_ERR, "Error message: %s", get_error_message(e));
    else if((rc = get_error_code(e)) != 0)
        syslog(LOG_ERR, "Possible error: %d %s\n", rc, strerror(rc));

    meta_vsyslog(LOG_ERR, fmt, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}
