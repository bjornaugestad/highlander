/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "internals.h"

struct cookie_tag {
    cstring name;
    cstring value;
    cstring domain;
    cstring path;
    cstring comment;
    int max_age;
    int secure;
    int version;
};

cookie cookie_new(void)
{
    cookie p;
    cstring arr[5];

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;

    if (!cstring_multinew(arr, 5)) {
        free(p);
        return NULL;
    }

    p->name = arr[0];
    p->value = arr[1];
    p->domain = arr[2];
    p->path = arr[3];
    p->comment = arr[4];
    p->max_age = MAX_AGE_NOT_SET;
    p->secure = 0;
    p->version = 1; /* default acc. to rfc2109 */

    return p;
}

void cookie_free(cookie p)
{
    if (p != NULL) {
        cstring_free(p->name);
        cstring_free(p->value);
        cstring_free(p->domain);
        cstring_free(p->path);
        cstring_free(p->comment);
        free(p);
    }
}

status_t cookie_set_name(cookie c, const char *s)
{
    assert(c != NULL);
    assert(s != NULL);

    return cstring_set(c->name, s);
}

status_t cookie_set_value(cookie c, const char *s)
{
    assert(c != NULL);
    assert(s != NULL);

    return cstring_set(c->value, s);
}

status_t cookie_set_comment(cookie c, const char *s)
{
    assert(c != NULL);
    assert(s != NULL);

    return cstring_set(c->comment, s);
}

status_t cookie_set_domain(cookie c, const char *s)
{
    assert(c != NULL);
    assert(s != NULL);

    return cstring_set(c->domain, s);
}

status_t cookie_set_path(cookie c, const char *s)
{
    assert(c != NULL);
    assert(s != NULL);

    return cstring_set(c->path, s);
}

void cookie_set_version(cookie c, int v)
{
    /* We only understand 0 and 1 */
    assert(v == 0 || v == 1);
    assert(c != NULL);

    c->version = v;
}

void cookie_set_secure(cookie c, int v)
{
    assert(c != NULL);
    assert(v == 0 || v == 1);

    c->secure = v;
}

void cookie_set_max_age(cookie c, int v)
{
    assert(c != NULL);

    c->max_age = v;
}

const char *cookie_get_name(cookie c)
{
    assert(c != NULL);
    return c_str(c->name);
}

const char *cookie_get_value(cookie c)
{
    assert(c != NULL);
    return c_str(c->value);
}

const char *cookie_get_comment(cookie c)
{
    assert(c != NULL);
    return c_str(c->comment);
}

const char *cookie_get_domain(cookie c)
{
    assert(c != NULL);
    return c_str(c->domain);
}

const char *cookie_get_path(cookie c)
{
    assert(c != NULL);
    return c_str(c->path);
}

int cookie_get_version(cookie c)
{
    assert(c != NULL);
    return c->version;
}

int cookie_get_secure(cookie c)
{
    assert(c != NULL);
    return c->secure;
}

int cookie_get_max_age(cookie c)
{
    assert(c != NULL);
    return c->max_age;
}

/* TODO: Replace cstring with char* and drop the meta_error.
 * Reason? Well, it may be possible to flood the incoming data
 * in some way, so we reduce the use of resizeable strings.
 * With fixed buffer sizes, we can always return HTTP_BAD_REQUEST
 * so we do not need meta_error.
 */
static status_t get_cookie_attribute(
    const char *s,
    const char *attribute,	/* $Version, $Path, $Secure or $Domain */
    cstring value,		/* Store result here */
    meta_error e)
{
    char *location;
    size_t i;

    assert(attribute != NULL);

    if ((location = strstr(s, attribute)) == NULL)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    /* Remove ws, = and first " */
    location += strlen(attribute);
    if ((i = strspn(location, " \t=\"")) == 0)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    location += i;
    while (*location != '\0' && *location != '"') {
        if (!cstring_charcat(value, *location++))
            return set_os_error(e, ENOMEM);
    }

    if (*location != '"')
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    return success;
}

/* Cookies are defined in rfc2109
 * Format is: (copy from rfc)
 * The syntax for the header is:
 *
 *	cookie = "Cookie:" cookie-version
 *			 1*((";" | ",") cookie-value)
 *	cookie-value = NAME "=" VALUE [";" path] [";" domain]
 *	cookie-version	= "$Version" "=" value
 *	NAME   = attr
 *	VALUE  = value
 *	path   = "$Path" "=" value
 *	domain = "$Domain" "=" value
 *
 *	NOTES:
 *	Now for the fun part :-(
 *	a) Netscape Communicator 4.72 sends no $Version
 *	b) Lynx sends cookie2 as fieldname if no version is
 *	included in outgoing cookie.
 *	c) Lynx does not send Path and Domain back
 *	d) kfm (KDE File Manager) looks good!
 *	e) (NEW: 2003-01-06) Some programs, e.g. siege, sends empty
 *	   cookie tags. This is illegal, but I assume that other browsers
 *	   may do this as well. I have added the SUPPORT_EMPTY_COOKIES
 *	   so that we can switch the support for illegal cookie tags on and off.
 *	   To summarize; we now support "Cookie: \r\n".
 */
status_t parse_cookie(http_request req, const char *value, meta_error e)
{
    /* Locate version */
    char *s;

#define SUPPORT_EMPTY_COOKIES 1
#ifdef SUPPORT_EMPTY_COOKIES
    if (strcmp(value, "") == 0) {
        return success;
    }
#endif

    if ((s = strstr(value, "$Version")) != NULL)
        return parse_new_cookie(req, value, e);
    else
        return parse_old_cookie(req, value, e);
}

static const char *find_first_non_space(const char *s)
{
    while (*s != '\0' && (*s == ' ' || *s == '\t'))
        s++;

    return s;
}

static status_t parse_cookie_attr(
    cookie c,
    const char *input,
    const char *look_for,
    status_t (*set_attr)(cookie, const char*),
    meta_error e)
{
    cstring str;

    if ((str = cstring_new()) == NULL)
        return set_os_error(e, ENOMEM);

    if (!get_cookie_attribute(input, look_for, str, e)) {
        cstring_free(str);
        return 0;
    }

    set_attr(c, c_str(str));
    cstring_free(str);
    return success;
}

/*
 * look for name=value as in $Version="1";foo=bar  and
 * add name and value to the cookie .
 * Returns 1 on success, else a http error do
 */
static status_t parse_new_cookie_name(cookie c, const char *input, meta_error e)
{
    const char *s, *s2;
    cstring str;

    if ((s = strchr(input, ';')) == NULL)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    /* skip ; and white space (if any) */
    if ((s = find_first_non_space(++s)) == NULL) {
        /* If all we had was ws */
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    if ((s2 = strchr(s, '=')) == NULL) {
        /* Missing = in Name= */
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    if ((str = cstring_new()) == NULL)
        return set_os_error(e, errno);

    if (!cstring_nset(str, s, (size_t) (s2 - s + 1))) {
        cstring_free(str);
        return set_os_error(e, errno);
    }

    if (!cookie_set_name(c, c_str(str))) {
        cstring_free(str);
        return set_os_error(e, errno);
    }

    s = s2 + 1;
    cstring_recycle(str);
    if ((s2 = strchr(s, ';')) == NULL) {
        /* Missing ; in Name=value; */
        cstring_free(str);
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    if (!cstring_nset(str, s, (size_t)(s2 - s + 1))) {
        cstring_free(str);
        return set_os_error(e, errno);
    }

    if (!cookie_set_value(c, c_str(str))) {
        cstring_free(str);
        return set_os_error(e, errno);
    }

    cstring_free(str);
    return success;
}

static status_t parse_new_cookie_secure(cookie c, const char *value, meta_error e)
{
    int secure;
    cstring str;

    if ((str = cstring_new()) == NULL)
        return set_os_error(e, errno);

    if (!get_cookie_attribute(value, "$Secure", str, e)) {
        cstring_free(str);
        return 0;
    }

    secure = atoi(c_str(str));
    if (secure != 1 && secure != 0) {
        cstring_free(str);
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    cookie_set_secure(c, secure);
    cstring_free(str);
    return success;
}

static status_t parse_new_cookie_domain(cookie c, const char *value, meta_error e)
{
    return parse_cookie_attr(c, value, "$Domain", cookie_set_domain, e);
}

static status_t parse_new_cookie_path(cookie c, const char *value, meta_error e)
{
    return parse_cookie_attr(c, value, "$Path", cookie_set_path, e);
}

static status_t parse_new_cookie_version(cookie c, const char *value, meta_error e)
{
    int version;
    cstring str;

    if ((str = cstring_new()) == NULL)
        return set_os_error(e, ENOMEM);

    if (!get_cookie_attribute(value, "$Version", str, e)) {
        cstring_free(str);
        return 0;
    }

    version = atoi(c_str(str));
    cstring_free(str);

    if (version != 1)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    cookie_set_version(c, version);
    return success;
}

status_t parse_new_cookie(http_request req, const char *value, meta_error e)
{
    cookie c;

    assert(req != NULL);
    assert(value != NULL);

    if ((c = cookie_new()) == NULL)
        return set_os_error(e, ENOMEM);

    /* New cookies require this field! */
    if (!parse_new_cookie_version(c, value, e)) {
        cookie_free(c);
        return failure;
    }

    /* Now for the rest of the attributes */
    if (!parse_new_cookie_name(c, value, e)
    || !parse_new_cookie_path(c, value, e)
    || !parse_new_cookie_domain(c, value, e)
    || !parse_new_cookie_secure(c, value, e)) {
        cookie_free(c);
        return failure;
    }

    if (!request_add_cookie(req, c)) {
        set_os_error(e, errno);
        cookie_free(c);
        return failure;
    }

    return success;
}

/*
 * The old cookie format is (hopefully) name=value
 * where value may be quoted.
 */
status_t parse_old_cookie(http_request req, const char *input, meta_error e)
{
    cookie c = NULL;
    cstring name = NULL, value = NULL;

    if ((name = cstring_new()) == NULL
    ||	(value = cstring_new()) == NULL)
        goto memerr;

    while (*input != '\0' && *input != '=') {
        if (!cstring_charcat(name, *input++))
            goto memerr;
    }

    if (*input != '=') {
        cstring_free(name);
        cstring_free(value);
        return set_http_error(e, HTTP_400_BAD_REQUEST);
    }

    input++; /* Skip '=' */
    if (!cstring_set(value, input))
        goto memerr;

    if ((c = cookie_new()) == NULL)
        goto memerr;

    if (!cookie_set_name(c, c_str(name))
    || !cookie_set_value(c, c_str(value)))
        goto memerr;

    cstring_free(name);
    cstring_free(value);
    cookie_set_version(c, 0);
    if (!request_add_cookie(req, c)) {
        set_os_error(e, errno);
        cookie_free(c);
        return 0;
    }

    return success;

memerr:
    cstring_free(name);
    cstring_free(value);
    cookie_free(c);
    return set_os_error(e, ENOMEM);
}

status_t cookie_dump(cookie c, void *file)
{
    FILE *f = file;

    fprintf(f, "Name   :%s\n", c_str(c->name));
    fprintf(f, "Value  :%s\n", c_str(c->value));
    fprintf(f, "Domain :%s\n", c_str(c->domain));
    fprintf(f, "Path   :%s\n", c_str(c->path));
    fprintf(f, "Comment:%s\n", c_str(c->comment));
    fprintf(f, "Max-Age:%d\n", c->max_age);
    fprintf(f, "Secure :%d\n", c->secure);
    fprintf(f, "Version:%d\n", c->version);
    return success;
}


#ifdef CHECK_COOKIE

#include <string.h>

int main(void)
{
    cookie c;
    size_t i, niter = 10000;
    const char *name = "name";
    const char *value = "value";
    const char *domain = "DOMAIN";
    const char *path = "PATH";
    const char *comment = "THIS IS A COMMENT";
    int max_age = 0;
    int secure = 1;
    int version = 1;

    for (i = 0; i < niter; i++) {
        if ((c = cookie_new()) == NULL)
            return 77;

        if (!cookie_set_name(c, name)) return 77;
        if (!cookie_set_value(c, value)) return 77;
        if (!cookie_set_domain(c, domain)) return 77;
        if (!cookie_set_path(c, path)) return 77;
        if (!cookie_set_comment(c, comment)) return 77;
        cookie_set_max_age(c, max_age);
        cookie_set_secure(c, secure);
        cookie_set_version(c, version);

        if (strcmp(cookie_get_name(c), name) != 0) return 77;
        if (strcmp(cookie_get_value(c), value) != 0) return 77;
        if (strcmp(cookie_get_domain(c), domain) != 0) return 77;
        if (strcmp(cookie_get_path(c), path) != 0) return 77;
        if (strcmp(cookie_get_comment(c), comment) != 0) return 77;
        if (cookie_get_max_age(c) != max_age) return 77;
        if (cookie_get_secure(c) != secure) return 77;
        if (cookie_get_version(c) != version) return 77;

        cookie_free(c);
    }

    return 0;
}

#endif
