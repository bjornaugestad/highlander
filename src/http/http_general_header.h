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

#ifndef HTTP_GENERAL_HEADER_H
#define HTTP_GENERAL_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

/* The HTTP general header is shared between requests and responses
 * and contains a couple of fields. We store them in a separate
 * struct instead of in the request/response ADT's so that the parser
 * can be more generalized.
 */
typedef struct general_header_tag *general_header;

general_header general_header_new(void);
void general_header_free(general_header p);

int general_header_set_date(general_header p, time_t value, meta_error e);


#ifdef __cplusplus
}
#endif

#endif
