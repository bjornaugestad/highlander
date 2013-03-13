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

#ifndef META_ERROR_H
#define META_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
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
 * I therefore use an error class, meta_error, and report errors there.
 * Functions then must accept a meta_error* as a param and return -1
 * on failure and fill out the proper values in the param.
 */
#define META_ERROR_MESSAGE_MAX 1024
typedef struct meta_error_tag* meta_error;
meta_error meta_error_new(void);
void meta_error_free(meta_error e);

/**
 * All set_ functions return 0. This way we can write code
 * like "return set_http_error(e, HTTP_400_BAD_REQUEST);"
 * instead of
 *		set_http_error(e, HTTP_400_BAD_REQUEST);
 *		return 0;
 * It saves us lots of blank lines and braces.
 */
int set_tcpip_error(meta_error e, int val);
int set_http_error(meta_error e, int val);
int set_app_error(meta_error e, int val);
int set_os_error(meta_error e, int val);
int set_db_error(meta_error e, int val);
int set_other_error(meta_error e, int val);

int is_tcpip_error(meta_error e);
int is_protocol_error(meta_error e);
int is_app_error(meta_error e);
int is_os_error(meta_error e);
int is_db_error(meta_error e);
int is_other_error(meta_error e);
int get_error_code(meta_error e);

void set_error_message(meta_error e, const char *msg);
int has_error_message(meta_error e);
const char *get_error_message(meta_error e);


void die_with_error(meta_error e, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif

