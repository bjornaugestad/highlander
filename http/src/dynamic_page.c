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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "internals.h"

/* Stores info for one dynamic page */
struct dynamic_page_tag {
	cstring uri;
	page_attribute attr;
	PAGE_FUNCTION handler;
};

dynamic_page dynamic_new(const char* uri, PAGE_FUNCTION handler, page_attribute a)
{
	dynamic_page p;

	if ((p = malloc(sizeof *p)) == NULL)
		return NULL;

	p->handler = handler;
	p->attr = NULL;
	if ((p->uri = cstring_dup(uri)) == NULL) {
		free(p);
		return NULL;
	}

	if (a != NULL && (p->attr = attribute_dup(a)) == NULL) {
		cstring_free(p->uri);
		free(p);
		return NULL;
	}

	return p;
}

void dynamic_free(dynamic_page p)
{
	assert(NULL != p);

	if (p != NULL) {
		attribute_free(p->attr);
		cstring_free(p->uri);
		free(p);
	}
}

const char* dynamic_get_uri(dynamic_page p)
{
	assert(NULL != p);
	return c_str(p->uri);
}

int dynamic_set_uri(dynamic_page p, const char* value)
{
	assert(NULL != p);
	assert(NULL != value);

	return cstring_set(p->uri, value);
}

void dynamic_set_handler(dynamic_page p, PAGE_FUNCTION func)
{
	assert(NULL != p);
	assert(NULL != func);
	p->handler = func;
}

int dynamic_run(dynamic_page p, const http_request req, http_response response)
{
	assert(NULL != p);

	return (*p->handler)(req, response);
}

int dynamic_set_attributes(dynamic_page p, page_attribute a)
{
	attribute_free(p->attr);
	if ((p->attr = attribute_dup(a)) == NULL)
		return 0;

	return 1;
}

page_attribute dynamic_get_attributes(dynamic_page p)
{
	assert(NULL != p);
	return p->attr;
}

#ifdef CHECK_DYNAMIC_PAGE

int dummy(http_request rq, http_response rsp)
{
	return 0;
}

int main(void)
{
	dynamic_page p;
	size_t i, niter;

	for (i = 0; i < niter; i++) {
		if ((p = dynamic_new("/dummy_uru", dummy, NULL)) == NULL)
			return 77;

		dynamic_page_free(p);
	}

	return 0;
}
#endif

