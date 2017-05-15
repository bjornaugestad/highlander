/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <highlander.h>
#include <internals.h>

/*
 * TODO (2007-05-25):
 * - Add support for field-names in the no-cache and private header fields.
 * field-names are names of actual HTTP fields.
 *
 * - Add support for cache-extension. parse/set/get/send. cache-extension
 * fields are hard to parse as the value isn't specified in RFC2616. The
 * format is: token [ = (token | quoted-string)]
 * Example: Cache-Control: foo = bar, foobar, foz = "quoted string"
 * This example has 3 tokens that extend the Cache-Control field. Two has
 * values, 1 has no value. HOW TO PARSE:
 * 1. Read the token name.
 * 2. If the name is unknown, it is an extension field.
 * 2.1 Read until , or \r\n
 * 2.2 Put token and value in string variable. (Just copy it?)
 *	   or maybe in a map.

 * HOW TO GET/SET/SEND Cache-Extensions:
 * -If we settle for a string, let the user set a string directly
 * regardless of format.
 * - The get functions just returns a const char pointer to that string.
 * - A send function just sends the string.
 * - What about field-separating commas?
 * MORE ABOUT Cache-Extensions:
 * - Reread RFC2616, IIRC there was some note requiring us to
 * accept more or less anything (if we are a proxy).
 *
 */

/*
 * About general_header.c:
 * The General-Header is described in §4.5 and is supposed to
 * contain fields and values common to both HTTP requests and responses.
 * There are 9 fields: Cache-Control, Connection, Date, Pragma,
 * Trailer, Transfer-Encoding, Upgrade, Via and Warning.
 *
 * Some fields are easy to manage, e.g. Date. Others are a mess,
 * like Cache-Control and Pragma. They're a mess because they
 * contain subfields and subvalues. Some fields also overlap in
 * functionality, Cache-Control: no-cache equals Pragma: No-cache.
 */

/*
 * About Cache-Control:
 * The field is described in §14.9. It can have multiple
 * cache directives, either request directives or response-directives.
 * Some directives applies to both request and response, but with
 * different (optional) syntax. (How the hell did that happen?)
 * cache-request-directives are:
 *		 no-cache
 *		 no-store
 *		 max-age = delta-seconds
 *		 max-stale [ = delta-seconds ]
 *		 min-fresh = delta-seconds
 *		 no-transform
 *		 only-if-cached
 *		 cache-extension # Whee!
 *
 * cache-response-directives are:
 *		public
 *		private [ = "1#fieldname" ]
 *		no-cache [ = "1#fieldname" ]
 *		no-store
 *		no-transform
 *		must-revalidate
 *		proxy-revalidate
 *		max-age = delta-seconds
 *		s-maxage = delta-seconds
 *		cache-extension
 *
 * A cache-extension is: token=(token|quoted-string)
 */

/*
 * STATUS for the different fields and directions.
 * We need to know if we parse a request field and send a response field.
 * We also need to know if we have an API for the field.
-------------------------------------------------------------------------
Field:						set	get	parse	send	Comment
request:no-cache			x	x	x		x		parse_pragma() sets it too.
request:no-store			x	x	x		x
request:max-age				x	x	x		x
request:max-stale			x	x	x		x
request:min-fresh			x	x	x		x
request:no-transform		x	x	x		x
request:only-if-cached		x	x	x		x
request:cache-extension		0	0	0		0

response:public				x	x	0		x		No member variable
response:private			x	x	0		x		No member variable
response:no-cache			Shared with request
response:no-store			Shared with request
response:no-transform		Shared with request
response:must-revalidate	x	x	0		x		No member variable
response:proxy-revalidate	x	x	0		x		No member variable
response:max-age			Shared with request
response:s-maxage			x	x	0		x		No member variable
response:cache-extension
 */

/*
 * Questions and unsolved issues:
 * Q: How do we assert that we don't screw up semantically if
 * we choose to use the same fields for request and response cache settings?
 * I.e. We should never transmit a only-if-cached field. (Or should we? )
 * I.e. 2: We shall never parse a s-maxage field if we parse a request.

 * A: We don't care. It is the callers responsibility
 *
 */

typedef unsigned long flagtype;
#define GENERAL_HEADER_DATE_SET					( 0x01)
#define GENERAL_HEADER_TRAILER_SET				( 0x02)
#define GENERAL_HEADER_TRANSFER_ENCODING_SET	( 0x04)
#define GENERAL_HEADER_ONLY_IF_CACHED_SET		( 0x08)
#define GENERAL_HEADER_UPGRADE_SET				( 0x10)
#define GENERAL_HEADER_VIA_SET					( 0x20)
#define GENERAL_HEADER_WARNING_SET				( 0x40)
#define GENERAL_HEADER_CONNECTION_SET			( 0x80)
#define GENERAL_HEADER_PRAGMA_SET				( 0x100)

/* Cache control fields */
#define GENERAL_HEADER_NO_CACHE_SET				( 0x200)
#define GENERAL_HEADER_NO_STORE_SET				( 0x400)
#define GENERAL_HEADER_MAX_AGE_SET				( 0x800)
#define GENERAL_HEADER_MAX_STALE_SET			( 0x1000)
#define GENERAL_HEADER_MIN_FRESH_SET			( 0x2000)
#define GENERAL_HEADER_NO_TRANSFORM_SET			( 0x4000)
#define GENERAL_HEADER_PUBLIC_SET				( 0x8000)
#define GENERAL_HEADER_PRIVATE_SET				( 0x10000)
#define GENERAL_HEADER_MUST_REVALIDATE_SET		( 0x20000)
#define GENERAL_HEADER_PROXY_REVALIDATE_SET		( 0x40000)
#define GENERAL_HEADER_S_MAXAGE_SET				( 0x80000)
#define GENERAL_HEADER_CACHE_EXTENSION_SET		( 0x100000)


struct general_header_tag {
    flagtype flags; /* See http_request.c for an explanation */

    /* Cache control flags/values , v1.1 §14.9 */
    #if 0
    /* We only need to flag these fields */
    int no_cache;				/* v1.0 §10.12 v1.1 §14.32 */
    int no_store;
    int no_transform;
    int only_if_cached;
    #endif
    int max_age;
    int s_maxage; /* v1.1 §  14.9.3 */
    int max_stale;
    int min_fresh;

    cstring connection;			/* v1.1 §14.10 */
    time_t date;				/* v1.0 §10.6 v1.1 §14.18 */
    cstring pragma;				/* §14.32 */
    cstring trailer;			/* v1.1 §14.40 */
    cstring transfer_encoding;	/* v1.1 §14.41 */
    cstring upgrade;			/* v1.1 §14.42 */
    cstring via;				/* v1.1 §14.45 */
    cstring warning;			/* v1.1 §14.46 */
};

static inline bool 
general_header_flag_is_set(general_header gh, flagtype flag)
{
    assert(gh != NULL);
    assert(flag > 0);

    return gh->flags & flag ? true : false;
}

static inline void 
general_header_set_flag(general_header gh, flagtype flag)
{
    assert(gh != NULL);
    assert(flag > 0);

    gh->flags |= flag;
}

static inline void 
general_header_clear_flags(general_header gh)
{
    assert(gh != NULL);
    gh->flags = 0;
}

void general_header_recycle(general_header p)
{
    general_header_clear_flags(p);
}

void general_header_free(general_header p)
{
    if (p != NULL) {
        cstring_free(p->connection);
        cstring_free(p->pragma);
        cstring_free(p->trailer);
        cstring_free(p->transfer_encoding);
        cstring_free(p->upgrade);
        cstring_free(p->via);
        cstring_free(p->warning);
        free(p);
    }
}

general_header general_header_new(void)
{
    general_header p;
    cstring arr[7];

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    if (!cstring_multinew(arr, sizeof arr / sizeof *arr)) {
        free(p);
        return NULL;
    }

    general_header_clear_flags(p);
    p->connection = arr[0];
    p->pragma = arr[1];
    p->trailer = arr[2];
    p->transfer_encoding = arr[3];
    p->upgrade = arr[4];
    p->via = arr[5];
    p->warning = arr[6];

    return p;
}


void general_header_set_date(general_header gh, time_t value)
{
    assert(gh != NULL);
    assert(value != (time_t)-1);

    gh->date = value;
    general_header_set_flag(gh, GENERAL_HEADER_DATE_SET);
}

status_t general_header_set_connection(general_header gh, const char* value)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!cstring_set(gh->connection, value))
        return failure;

    general_header_set_flag(gh, GENERAL_HEADER_CONNECTION_SET);
    return success;
}

status_t general_header_set_pragma(general_header gh, const char* value)
{
    assert(NULL !=	gh);
    assert(value != NULL);

    if (!cstring_set(gh->pragma, value))
        return failure;

    general_header_set_flag(gh, GENERAL_HEADER_PRAGMA_SET);
    return success;
}

status_t general_header_set_trailer(general_header gh, const char* value)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!cstring_set(gh->trailer, value))
        return failure;

    general_header_set_flag(gh, GENERAL_HEADER_TRAILER_SET);
    return success;
}

status_t general_header_set_transfer_encoding(general_header gh, const char* value)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!cstring_set(gh->transfer_encoding, value))
        return failure;

    general_header_set_flag(gh, GENERAL_HEADER_TRANSFER_ENCODING_SET);
    return success;
}

status_t general_header_set_upgrade(general_header gh, const char* value)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!cstring_set(gh->upgrade, value))
        return failure;

    general_header_set_flag(gh, GENERAL_HEADER_UPGRADE_SET);
    return success;
}

status_t general_header_set_via(general_header gh, const char* value)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!cstring_set(gh->via, value))
        return failure;

    general_header_set_flag(gh, GENERAL_HEADER_VIA_SET);
    return success;
}

status_t general_header_set_warning(general_header gh, const char* value)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!cstring_set(gh->warning, value))
        return failure;

    general_header_set_flag(gh, GENERAL_HEADER_WARNING_SET);
    return success;
}

void general_header_set_no_cache(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_NO_CACHE_SET);
}

void general_header_set_no_store(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_NO_STORE_SET);
}

void general_header_set_max_age(general_header gh, int value)
{
    assert(gh != NULL);

    gh->max_age = value;
    general_header_set_flag(gh, GENERAL_HEADER_MAX_AGE_SET);
}

void general_header_set_s_maxage(general_header gh, int value)
{
    assert(gh != NULL);

    gh->s_maxage = value;
    general_header_set_flag(gh, GENERAL_HEADER_S_MAXAGE_SET);
}

void general_header_set_max_stale(general_header gh, int value)
{
    assert(gh != NULL);

    gh->max_stale = value;
    general_header_set_flag(gh, GENERAL_HEADER_MAX_STALE_SET);
}

void general_header_set_min_fresh(general_header gh, int value)
{
    assert(gh != NULL);

    gh->min_fresh = value;
    general_header_set_flag(gh, GENERAL_HEADER_MIN_FRESH_SET);
}

void general_header_set_no_transform(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_NO_TRANSFORM_SET);
}

void general_header_set_only_if_cached(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_ONLY_IF_CACHED_SET);
}

void general_header_set_public(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_PUBLIC_SET);
}

void general_header_set_private(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_PRIVATE_SET);
}

void general_header_set_must_revalidate(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_MUST_REVALIDATE_SET);
}

void general_header_set_proxy_revalidate(general_header gh)
{
    assert(gh != NULL);
    general_header_set_flag(gh, GENERAL_HEADER_PROXY_REVALIDATE_SET);
}

bool general_header_get_no_cache(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_NO_CACHE_SET);
}

bool general_header_get_no_store(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_NO_STORE_SET);
}

int general_header_get_max_age(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_MAX_AGE_SET));
    return gh->max_age;
}

int general_header_get_s_maxage(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_S_MAXAGE_SET));
    return gh->s_maxage;
}

int general_header_get_max_stale(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_MAX_STALE_SET));
    return gh->max_stale;
}

int general_header_get_min_fresh(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_MIN_FRESH_SET));
    return gh->min_fresh;
}

bool general_header_get_public(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_PUBLIC_SET);
}

bool general_header_get_private(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_PRIVATE_SET);
}

bool general_header_get_must_revalidate(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_MUST_REVALIDATE_SET);
}

bool general_header_get_proxy_revalidate(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_PROXY_REVALIDATE_SET);
}

bool general_header_get_no_transform(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_NO_TRANSFORM_SET);
}

bool general_header_get_only_if_cached(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_ONLY_IF_CACHED_SET);
}

time_t general_header_get_date(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_DATE_SET));
    return gh->date;
}

const char* general_header_get_connection(general_header gh)
{
    assert(gh != NULL);
    if (!general_header_flag_is_set(gh, GENERAL_HEADER_CONNECTION_SET))
        return "";
    else
        return c_str(gh->connection);
}

const char* general_header_get_trailer(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_TRAILER_SET));
    return c_str(gh->trailer);
}

const char* general_header_get_transfer_encoding(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_TRANSFER_ENCODING_SET));
    return c_str(gh->transfer_encoding);
}

const char* general_header_get_upgrade(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_UPGRADE_SET));
    return c_str(gh->upgrade);
}

const char* general_header_get_via(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_VIA_SET));
    return c_str(gh->via);
}

const char* general_header_get_warning(general_header gh)
{
    assert(gh != NULL);
    assert(general_header_flag_is_set(gh, GENERAL_HEADER_WARNING_SET));
    return c_str(gh->warning);
}

bool general_header_no_cache_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_NO_CACHE_SET);
}

bool general_header_no_store_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_NO_STORE_SET);
}

bool general_header_max_age_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_MAX_AGE_SET);
}

bool general_header_connection_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_CONNECTION_SET);
}

bool general_header_pragma_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_PRAGMA_SET);
}

bool general_header_max_stale_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_MAX_STALE_SET);
}

bool general_header_min_fresh_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_MIN_FRESH_SET);
}

bool general_header_no_transform_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_NO_TRANSFORM_SET);
}

bool general_header_only_if_cached_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_ONLY_IF_CACHED_SET);
}

bool general_header_date_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_DATE_SET);
}

bool general_header_trailer_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_TRAILER_SET);
}

bool general_header_transfer_encoding_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_TRANSFER_ENCODING_SET);
}

bool general_header_upgrade_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_UPGRADE_SET);
}

bool general_header_via_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_VIA_SET);
}

bool general_header_warning_isset(general_header gh)
{
    assert(gh != NULL);
    return general_header_flag_is_set(gh, GENERAL_HEADER_WARNING_SET);
}

bool general_header_is_chunked_message(general_header gh)
{
    if (!general_header_transfer_encoding_isset(gh))
        return false;

    return cstring_casecompare(gh->transfer_encoding, "chunked") == 0;
}

static inline status_t send_connection(general_header gh, connection conn)
{
    return http_send_field(conn, "Connection: ", gh->connection);
}

static inline status_t send_pragma(general_header p, connection conn)
{
    return http_send_field(conn, "Pragma: ", p->pragma);
}

static inline status_t send_trailer(general_header gh, connection conn)
{
    return http_send_field(conn, "Trailer: ", gh->trailer);
}

static inline status_t send_transfer_encoding(general_header gh, connection conn)
{
    return http_send_field(conn, "Transfer-Encoding: ", gh->transfer_encoding);
}

static inline status_t send_upgrade(general_header gh, connection conn)
{
    return http_send_field(conn, "Upgrade: ", gh->upgrade);
}

static inline status_t send_via(general_header gh, connection conn)
{
    return http_send_field(conn, "Via: ", gh->via);
}

static inline status_t send_warning(general_header gh, connection conn)
{
    return http_send_field(conn, "Warning: ", gh->warning);
}

static inline status_t send_date(general_header gh, connection conn)
{
    return http_send_date(conn, "Date: ", gh->date);
}

static inline int cachecontrol_field_set(general_header gh)
{
    int i
        = general_header_flag_is_set(gh, GENERAL_HEADER_NO_CACHE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_NO_STORE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_MAX_AGE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_MAX_STALE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_MIN_FRESH_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_NO_TRANSFORM_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_PUBLIC_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_PRIVATE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_MUST_REVALIDATE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_PROXY_REVALIDATE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_S_MAXAGE_SET)
        + general_header_flag_is_set(gh, GENERAL_HEADER_CACHE_EXTENSION_SET);

    return i;
}

static inline status_t send_cachecontrol(general_header gh, connection conn)
{
    int nfields = cachecontrol_field_set(gh);

    if (!http_send_string(conn, "Cache-Control: "))
        return 0;

    if (general_header_flag_is_set(gh, GENERAL_HEADER_NO_CACHE_SET)) {
        if (!http_send_string(conn, "no-cache"))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_NO_STORE_SET)) {
        if (!http_send_string(conn, "no-store"))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;
        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_MAX_AGE_SET)) {
        if (!http_send_ulong(conn, "max-age=", gh->max_age))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_MAX_STALE_SET)) {
        if (!http_send_ulong(conn, "max-stale=", gh->max_stale))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_MIN_FRESH_SET)) {
        if (!http_send_ulong(conn, "min-fresh=", gh->min_fresh))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_NO_TRANSFORM_SET)) {
        if (!http_send_string(conn, "no-transform"))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_PUBLIC_SET)) {
        if (!http_send_string(conn, "public"))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_PRIVATE_SET)) {
        if (!http_send_string(conn, "private"))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_MUST_REVALIDATE_SET)) {
        if (!http_send_string(conn, "must-revalidate"))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_PROXY_REVALIDATE_SET)) {
        if (!http_send_string(conn, "proxy-revalidate"))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_S_MAXAGE_SET)) {
        if (!http_send_ulong(conn, "s-maxage=", gh->s_maxage))
            return 0;

        if (nfields && !http_send_string(conn, ", "))
            return 0;

        nfields--;
    }

    if (general_header_flag_is_set(gh, GENERAL_HEADER_CACHE_EXTENSION_SET)) {
    }

    assert(nfields == 0);
    return success;
}

status_t general_header_send_fields(general_header gh, connection c)
{
    status_t rc = success;
    size_t i, nelem;

    static const struct {
        size_t flag;
        status_t (*func)(general_header, connection);
    } fields[] = {
        { GENERAL_HEADER_PRAGMA_SET,			send_pragma },
        { GENERAL_HEADER_DATE_SET,				send_date },
        { GENERAL_HEADER_CONNECTION_SET,		send_connection },
        { GENERAL_HEADER_TRAILER_SET,			send_trailer },
        { GENERAL_HEADER_TRANSFER_ENCODING_SET,	send_transfer_encoding },
        { GENERAL_HEADER_UPGRADE_SET,			send_upgrade },
        { GENERAL_HEADER_VIA_SET,				send_via },
        { GENERAL_HEADER_WARNING_SET,			send_warning }
    };


    nelem = sizeof fields / sizeof *fields;
    for (i = 0; i < nelem; i++) {
        if (general_header_flag_is_set(gh, fields[i].flag))
            if ((rc = fields[i].func(gh, c)) == failure)
                break;
    }

    /*
     * We must also send the cache control fields.
     * They're treated a bit special, because 0..n fields are part
     * of 0..1 header field, the Cache-Control field. So if 1..n
     * fields are set, we send the Cache-Control field along with
     * all appropriate values.
     */
    if (rc && cachecontrol_field_set(gh))
        rc = send_cachecontrol(gh, c);

    return rc;
}

/* General header handlers */
/* Local helper for parse_cache_control. Needed since
 * a lot of code is duplicated inside and after the loop
 * The s argument must/should point to a legal request-directive
 * to be understood.
 * Returns success if OK, even if the directive wasn't understood.
 * This is to 'accept' extensions from 14.9.6
 */
static status_t set_cache_control(general_header gh, const char* s, error e)
{
    /*
     * We have 2 types of cache-request-directives, with and
     * without a numeric argument. We ignore extensions for now.
     */
    static const struct {
        const char* directive;
        void (*func)(general_header);
    } type1[] = {
        { "no-cache",		general_header_set_no_cache, },
        { "no-store",		general_header_set_no_store, },
        { "no-transform",	general_header_set_no_transform, },
        { "only-if-cached",	general_header_set_only_if_cached, },
    };

    static const struct {
        const char* directive;
        void (*func)(general_header, int);
    } type2[] = {
        { "max-age",	general_header_set_max_age, },
        { "max-stale",	general_header_set_max_stale, },
        { "min-fresh",	general_header_set_min_fresh, },
    };

    size_t i;

    /* Now look for type1 request-directives */
    for (i = 0; i < sizeof type1 / sizeof *type1; i++) {
        if (strstr(s, type1[i].directive) == s) {
            /* NOTE: There MAY slip in a bug here, in case
             * a new directive starts with the same name
             * as an existing directive. We ignore that now,
             * then we don't have to extract the name from
             * the string 'value'
             */
            (*type1[i].func)(gh);
            return success;
        }
    }

    /* Not a type1 directive, try type2 */
    for (i = 0; i < sizeof type2 / sizeof *type2; i++) {
        if (strstr(s, type2[i].directive) == s) {
            /* NOTE: Same 'bug' as above */
            char *eq = strchr(s, '=');
            int arg;

            /* Could not find = as in NAME=value */
            if (NULL == eq)
                return set_http_error(e, HTTP_400_BAD_REQUEST);

            /* Skip = and convert arg to integer */
            eq++;
            arg = -1;
            arg = atoi(eq);
            if (-1 == arg) {
                /* Conversion error. NOTE: How about strtol() instead? */
                return set_http_error(e, HTTP_400_BAD_REQUEST);
            }

            /* Call function and continue */
            (*type2[i].func)(gh, arg);
            return success;
        }
    }

    /* Not found */
    return success;
}

static inline status_t parse_transfer_encoding(general_header gh, const char* value, error e)
{
    if (!general_header_set_transfer_encoding(gh, value))
        return set_os_error(e, errno);

    return success;
}

static inline status_t parse_pragma(general_header gh, const char* value, error e)
{
    UNUSED(e);

    /* The only pragma we understand is no-cache */
    if (strstr(value, "no-cache") == value)
        general_header_set_no_cache(gh);

    /* Silently ignore unknown pragmas */
    return success;
}

/*
 * Warnings, from 14.46, look like this:
 *	Warning : warn-code SP warn-agent SP warn-text [SP warn-date]
 * We have already parsed the name and value hopefully contains
 * code+agent+text+optional date
 * Do we care? No, not really. Just store the warning.
 * A response may even contain more than one warning. Do we care? We're neither
 * a client nor a proxy ATM, so just store the value.
 */
static inline status_t parse_warning(general_header gh, const char* value, error e)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!general_header_set_warning(gh, value))
        return set_os_error(e, errno);

    return success;
}

static status_t
parse_cache_control(general_header gh, const char* value, error e)
{
    char* s;

    /* From rfc2616: Legal cache-control directives in a request
    cache-request-directive =
           "no-cache"						   ; Section 14.9.1
         | "no-store"						   ; Section 14.9.2
         | "max-age" "=" delta-seconds		   ; Section 14.9.3, 14.9.4
         | "max-stale" [ "=" delta-seconds ]   ; Section 14.9.3
         | "min-fresh" "=" delta-seconds	   ; Section 14.9.3
         | "no-transform"					   ; Section 14.9.5
         | "only-if-cached"					   ; Section 14.9.4
         | cache-extension					   ; Section 14.9.6
    */

    assert(gh != NULL);
    assert(value != NULL);

    /* Loop through the values looking for separating space.
     * Then look for the actual word
     */
    for (;*value != '\0';) {
        if ((s = strchr(value, ' ')) == NULL)
            break;

        /* Skip the space */
        s++;
        if (!set_cache_control(gh, s, e))
            return 0;

        value = s + 1;
    }

    /* Remember the last argument */
    return set_cache_control(gh, value, e);
}

static inline status_t
parse_date(general_header gh, const char* value, error e)
{
    time_t d;
    assert(gh != NULL);
    assert(value != NULL);

    /* Parse date and create a time_t */
    if ((d = parse_rfc822_date(value)) == -1)
        return set_http_error(e, HTTP_400_BAD_REQUEST);

    general_header_set_date(gh, d);
    return success;
}

/*
 * We only accept "close" or "keep-alive". Other values are regarded as invalid.
 * Do we report 400 or do we just ignore the values?
 * We start off being strict.
 * Update 20070918: Being strict is not the best solution. From now on
 * we accept "keep-alive" and any other value is interpreted as "close".
 */
static inline status_t parse_connection(general_header gh, const char* value, error e)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (strcasecmp(value, "keep-alive"))
        value = "close";

    if (!general_header_set_connection(gh, value))
        return set_os_error(e, errno);

    return success;
}

static inline status_t parse_trailer(general_header gh, const char* value, error e)
{
    assert(gh != NULL);
    assert(value != NULL);

    if (!general_header_set_trailer(gh, value))
        return set_os_error(e, errno);

    return success;
}

static inline status_t parse_upgrade(general_header gh, const char* value, error e)
{
    assert(gh != NULL);
    assert(value != NULL);

    /* Since we only understand http 1.0 and 1.1, I see
     * no reason whatsoever to support Upgrade.
     *
     * NOTE: Sat Apr 28 18:51:36 CEST 2001
     * Hmmh, maybe we should? How else do we support SSL/SHTTP?
     * If we decide to support Upgrade, the proper return status is
     *		101 Switching Protocols
     */
    if (!general_header_set_upgrade(gh, value))
        return set_os_error(e, errno);

    return success;
}

static inline status_t parse_via(general_header gh, const char* value, error e)
{
    assert(gh != NULL);
    assert(value != NULL);

    /* NOTE: This is incorrect, we may receive multiple Via: 's */
    if (!general_header_set_via(gh, value))
        return set_os_error(e, errno);

    return success;
}

static const struct {
    const char* name;
    status_t (*handler)(general_header gh, const char* value, error e);
} general_header_fields[] = {
    { "cache-control",		parse_cache_control },
    { "date",				parse_date },
    { "pragma",				parse_pragma },
    { "connection",			parse_connection },
    { "trailer",			parse_trailer },
    { "transfer-encoding",	parse_transfer_encoding },
    { "upgrade",			parse_upgrade },
    { "via",				parse_via },
    { "warning",			parse_warning }
};

/* Return an index in the general header array,
 * or -1 if the field was not found. */
int find_general_header(const char* name)
{
    int i, nelem = sizeof general_header_fields / sizeof *general_header_fields;
    for (i = 0; i < nelem; i++) {
        if (strcmp(general_header_fields[i].name, name) == 0)
            return i;
    }

    return -1;
}
status_t parse_general_header(int idx, general_header gh, const char* value, error e)
{
    assert(idx >= 0);
    assert((size_t)idx < sizeof general_header_fields / sizeof *general_header_fields);

    return general_header_fields[idx].handler(gh, value, e);
}

void general_header_dump(general_header gh, void *file)
{
    FILE *f = file;
    char date[100];

    assert(f != NULL);
    assert(gh != NULL);

    fprintf(f, "General Header fields\n");
    if (general_header_max_age_isset(gh))
        fprintf(f, "\tmax_age: %d\n", gh->max_age);

    if (general_header_flag_is_set(gh, GENERAL_HEADER_S_MAXAGE_SET))
        fprintf(f, "\ts_maxage: %d\n", gh->s_maxage);

    if (general_header_max_stale_isset(gh))
        fprintf(f, "\tmax_stale: %d\n", gh->max_stale);

    if (general_header_min_fresh_isset(gh))
        fprintf(f, "\tmin_fresh: %d\n", gh->min_fresh);

    if (general_header_connection_isset(gh))
        fprintf(f, "\tconnection: %s\n", c_str(gh->connection));

    if (general_header_date_isset(gh))
        fprintf(f, "\tdate: %s", ctime_r(&gh->date, date));

    if (general_header_pragma_isset(gh))
        fprintf(f, "\tpragma: %s\n", c_str(gh->pragma));

    if (general_header_transfer_encoding_isset(gh))
        fprintf(f, "\ttransfer_encoding: %s\n", c_str(gh->transfer_encoding));

    if (general_header_upgrade_isset(gh))
        fprintf(f, "\tupgrade: %s\n", c_str(gh->upgrade));

    if (general_header_via_isset(gh))
        fprintf(f, "\tvia: %s\n", c_str(gh->via));

    if (general_header_warning_isset(gh))
        fprintf(f, "\twarning: %s\n", c_str(gh->warning));
}
