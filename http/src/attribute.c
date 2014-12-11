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

#include "internals.h"

struct page_attribute_tag {
	cstring media_type;
	cstring language;
	cstring charset;
	cstring encoding;
};

page_attribute attribute_new(void)
{
	page_attribute p;
	cstring arr[4];

	if ((p = calloc(1, sizeof *p)) == NULL)
		return NULL;

	if (!cstring_multinew(arr, sizeof arr / sizeof *arr)) {
		free(p);
		return NULL;
	}

	p->media_type = arr[0];
	p->language = arr[1];
	p->charset = arr[2];
	p->encoding = arr[3];

	return p;
}

void attribute_free(page_attribute a)
{
	if (a != NULL) {
		cstring_free(a->media_type);
		cstring_free(a->language);
		cstring_free(a->charset);
		cstring_free(a->encoding);
		free(a);
	}
}

page_attribute attribute_dup(page_attribute a)
{
	page_attribute p;

	if ((p = attribute_new()) == NULL)
		return NULL;

	if (!cstring_set(p->language, c_str(a->language))
	|| !cstring_set(p->charset, c_str(a->charset))
	|| !cstring_set(p->encoding, c_str(a->encoding))
	|| !cstring_set(p->media_type, c_str(a->media_type))) {
		attribute_free(p);
		return NULL;
	}

	return p;
}

int attribute_set_media_type(page_attribute a, const char* value)
{
	return cstring_set(a->media_type, value);
}

int attribute_set_language(page_attribute a, const char* value)
{
	return cstring_set(a->language, value);
}

int attribute_set_charset(page_attribute a, const char* value)
{
	return cstring_set(a->charset, value);
}

int attribute_set_authorization(page_attribute a, const char* value)
{
	UNUSED(a);
	UNUSED(value);
	return 0;
}

int attribute_set_encoding(page_attribute a, const char* value)
{
	return cstring_set(a->encoding, value);
}

const char* attribute_get_media_type(page_attribute a)
{
	return c_str(a->media_type);
}

const char* attribute_get_language(page_attribute a)
{
	return c_str(a->language);
}

const char* attribute_get_charset(page_attribute a)
{
	return c_str(a->charset);
}

const char* attribute_get_encoding(page_attribute a)
{
	return c_str(a->encoding);
}

#ifdef CHECK_ATTRIBUTE
#include <string.h>

int main(void)
{
	page_attribute a, b;
	size_t i, niter = 10000;
	const char* media_type = "text/html";
	const char* language = "en_uk";
	const char* charset = "iso8859-1";
	const char* encoding = "gzip";

	for (i = 0; i < niter; i++) {
		if ((a = attribute_new()) == NULL)
			return 77;

		if (!attribute_set_media_type(a, media_type))
			return 77;

		if (!attribute_set_language(a, language))
			return 77;

		if (!attribute_set_charset(a, charset))
			return 77;

		if (!attribute_set_encoding(a, encoding))
			return 77;

		if (strcmp(attribute_get_media_type(a), media_type) != 0)
			return 77;

		if (strcmp(attribute_get_language(a), language) != 0)
			return 77;

		if (strcmp(attribute_get_charset(a), charset) != 0)
			return 77;

		if (strcmp(attribute_get_encoding(a), encoding) != 0)
			return 77;

		if ((b = attribute_dup(a)) == NULL)
			return 77;

		if (strcmp(attribute_get_media_type(b), media_type) != 0)
			return 77;

		if (strcmp(attribute_get_language(b), language) != 0)
			return 77;

		if (strcmp(attribute_get_charset(b), charset) != 0)
			return 77;

		if (strcmp(attribute_get_encoding(b), encoding) != 0)
			return 77;

		attribute_free(a);
		attribute_free(b);
	}

	return 0;
}

#endif
