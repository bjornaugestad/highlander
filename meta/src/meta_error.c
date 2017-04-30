/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#include <meta_common.h>
#include <meta_error.h>
#include <meta_misc.h>

enum error_group {
    meg_unknown,
    meg_tcpip,		/* network related error */
    meg_protocol,	/* Protocol, e.g. HTTP, syntax/semantic error */
    meg_app,		/* Some application error, unable to handle */
    meg_os,			/* Some call to the OS failed */
    meg_db,			/* some database error */
    meg_other		/* Other errors */
};

/*
 * Implementation of the error ADT.
 */
struct error_tag {
    enum error_group group;
    int code;
    char message[META_ERROR_MESSAGE_MAX + 1];
};


error error_new(void)
{
    error e;

    e = calloc(1, sizeof *e);
    return e;
}

void error_free(error e)
{
    free(e);
}

status_t set_tcpip_error(error e, int val)
{
    assert(e != NULL);

    e->group = meg_tcpip;
    e->code = val;

    return failure;
}

status_t set_http_error(error e, int val)
{
    if (e != NULL) {
        e->group = meg_protocol;
        e->code = val;
    }

    return failure;
}

status_t set_app_error(error e, int val)
{
    if (e != NULL) {
        e->group = meg_app;
        e->code = val;
    }

    return failure;
}

status_t set_os_error(error e, int val)
{
    if (e != NULL) {
        e->group = meg_os;
        e->code = val;
    }

    return failure;
}

status_t set_db_error(error e, int val)
{
    if (e != NULL) {
        e->group = meg_db;
        e->code = val;
    }

    return failure;
}

status_t set_other_error(error e, int val)
{
    if (e != NULL) {
        e->group = meg_other;
        e->code = val;
    }

    return failure;
}

int is_tcpip_error(error e)
{
    assert(e != NULL);
    return e->group == meg_tcpip;
}

int is_db_error(error e)
{
    assert(e != NULL);
    return e->group == meg_db;
}

int is_protocol_error(error e)
{
    assert(e != NULL);
    return e->group == meg_protocol;
}

int is_app_error(error e)
{
    assert(e != NULL);
    return e->group == meg_app;
}

int is_os_error(error e)
{
    assert(e != NULL);
    return e->group == meg_os;
}

int is_other_error(error e)
{
    assert(e != NULL);
    return e->group == meg_other;
}

int get_error_code(error e)
{
    assert(e != NULL);
    return e->code;
}


void set_error_message(error e, const char *msg)
{
    if (e != NULL) {
        strncpy(e->message, msg, sizeof e->message);
        e->message[sizeof e->message - 1] = '\0';
    }
}

int has_error_message(error e)
{
    assert(e != NULL);
    return strlen(e->message) > 0;
}

const char *get_error_message(error e)
{
    assert(e != NULL);
    return e->message;

}

void die_with_error(error e, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);

    if (is_tcpip_error(e))
        syslog(LOG_ERR, "A tcp/ip error has occured");
    else if (is_protocol_error(e))
        syslog(LOG_ERR, "A protocol error has occured");
    else if (is_app_error(e))
        syslog(LOG_ERR, "A application error has occured");
    else if (is_os_error(e))
        syslog(LOG_ERR, "A os error has occured");
    else if (is_db_error(e))
        syslog(LOG_ERR, "A database error has occured");
    else if (is_other_error(e))
        syslog(LOG_ERR, "An unknown error has occured");

    if (has_error_message(e))
        syslog(LOG_ERR, "Error message: %s", get_error_message(e));
    else if ((rc = get_error_code(e)) != 0)
        syslog(LOG_ERR, "Possible error: %d %s\n", rc, strerror(rc));

    meta_vsyslog(LOG_ERR, fmt, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}
