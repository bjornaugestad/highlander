/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
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
