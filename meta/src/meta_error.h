/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_ERROR_H
#define META_ERROR_H

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Generalized error handling.
 *
 * There can be multiple sources of errors in a server like metaweb.
 * 1. Socket errors due to network failure/hacking
 * 2. HTTP errors due to hacking or bugs
 * 3. Logical errors in metaweb-based apps
 * 4. OS errors such as ENOMEM, ENOENT
 * 5. Database errors if one is involved
 *
 * All these different errors must be handled, and handled well.
 * Some functions has to handle all of them, and currently have
 * a hard time doing so. The main problem is that it is hard to
 * store info about both the error code and the error group
 * in just one int (returned from a function).
 *
 * I therefore use an error class, error, and report errors there.
 * Functions then must accept a error* as a param and return -1
 * on failure and fill out the proper values in the param.
 */
#define META_ERROR_MESSAGE_MAX 1024
typedef struct error_tag* error;
error error_new(void);
void error_free(error e);

/*
 * All set_ functions return failure. This way we can write code
 * like "return set_http_error(e, HTTP_400_BAD_REQUEST);"
 * instead of
 *		set_http_error(e, HTTP_400_BAD_REQUEST);
 *		return failure;
 * It saves us lots of blank lines and braces.
 */
status_t set_tcpip_error(error e, int val);
status_t set_http_error(error e, int val);
status_t set_app_error(error e, int val);
status_t set_os_error(error e, int val);
status_t set_db_error(error e, int val);
status_t set_other_error(error e, int val);

int is_tcpip_error(error e);
int is_protocol_error(error e);
int is_app_error(error e);
int is_os_error(error e);
int is_db_error(error e);
int is_other_error(error e);
int get_error_code(error e);

void set_error_message(error e, const char *msg);
int has_error_message(error e);
const char *get_error_message(error e);


void die_with_error(error e, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
