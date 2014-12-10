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

/*
 * File contains misc functions, some to replace non-ansi functions
 * like strcasecmp()
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <meta_misc.h>

int string2size_t(const char *s, size_t *val)
{
	assert(s != NULL);
	assert(val != NULL);

	*val = 0;
	while (*s) {
		if (isdigit((int)*s))
			*val = (*val * 10) + (*s - '0');
		else
			return 0;

		s++;
	}
	return 1;
}

int find_word(const char *s, size_t iWord)
{
	const char *string = s;

	/* Loop 0..n times to skip words */
	assert(s != NULL);
	while (iWord--) {
		/* Skip until one space */
		while (*string != '\0' && *string != ' ')
			string++;

		/* Skip the space(even multiple) */
		while (*string != '\0' && *string == ' ')
			string++;
	}

	/* if index out of range */
	if (*string == '\0')
		return -1;

	return string - s;
}

int get_word_count(const char *s)
{
	int n = 0;
	int last_was_space = 0;

	assert(s != NULL);

	/* Remove leading ws */
	while (*s == ' ')
		s++;

	if (*s != '\0')
		n++;

	while (*s != '\0') {
		if (*s == ' ') {
			if (!last_was_space)
				n++;
			last_was_space = 1;
		}
		else
			last_was_space = 0;

		s++;
	}

	return n;
}

int get_word_from_string(
	const char *src,
	char dest[],
	size_t destsize,
	size_t iWord)		/* zero-based index of dest to copy */
{
	int i;

	assert(src != NULL);
	assert(dest != NULL);
	assert(destsize > 1);

	i = find_word(src, iWord);

	/* if index out of range */
	if (i == -1)
		return 0;

	/* copy the word */
	src += i;
	return copy_word(src, dest, ' ', destsize);
}

int copy_word(
	const char *src,
	char dest[],
	int separator,
	size_t destsize)
{
	size_t i = 0;

	assert(src != NULL);
	assert(dest != NULL);
	assert(separator != '\0');
	assert(destsize > 0);

	while (*src != '\0' && *src != separator && i < destsize)
		dest[i++] = *src++;

	if (i == destsize)
		return 0;

	dest[i] = '\0';
	return 1;
}

void remove_trailing_newline(char *s)
{
	size_t i;

	assert(s != NULL);

	i = strlen(s);
	if (i > 0) {
		i--;
		if (s[i] == '\n')
			s[i] = '\0';
	}
}

int get_extension(const char *src, char *dest, size_t destsize)
{
	const char *end;
	size_t i = destsize - 1;
	int found = 0;

	assert(src != NULL);
	assert(dest != NULL);
	assert(destsize > 1);

	end = src + strlen(src);
	dest[i] = '\0';

	while (end >= src && i > 0) {
		if (*end == '.') {
			found = 1;
			break;
		}

		dest[i--] = *end--;
	}

	if (i == 0) {
		errno = ENOSPC;
		return 0;
	}
	else if (found) {
		memmove(dest, &dest[i + 1], destsize - i);
	}
	else
		*dest = '\0';

	return 1;
}


/*
 * This is probably bad, but the need for speed forces us to
 * do it this way. I cannot see any security issues unless the
 * user of Highlander actually wants it to fail, so here we go.
 * Decide the mime type for a file, based on extension.
 */

const char *get_mime_type(const char *filename)
{
	static struct {
		const char *ext;
		const char *mime;
	} map[] = {
		{ "css",	"text/css" },
		{ "html",	"text/html" },
		{ "htm",	"text/html" },
		{ "c",		"text/plain" },
		{ "cpp",	"text/plain" },
		{ "cxx",	"text/plain" },
		{ "h",		"text/plain" },
		{ "java",	"text/plain" },
		{ "txt",	"text/plain" },
		{ "xml",	"text/xml" },
		{ "rtf",	"text/rtf" },
		{ "sgml",	"text/sgml" },

		{ "jpeg",	"image/jpeg" },
		{ "jpg",	"image/jpeg" },
		{ "png",	"image/png" },
		{ "tiff",	"image/tiff" },
		{ "gif",	"image/gif" },
	};

	size_t i, nelem = sizeof(map) / sizeof(map[0]);
	char ext[100];

	if (get_extension(filename, ext, sizeof ext)) {
		for (i = 0; i < nelem; i++) {
			if (strcmp(map[i].ext, ext) == 0) {
				return map[i].mime;
			}
		}
	}

	return "application/octet-stream";
}


#ifdef CHECK_MISCFUNC
int main(void)
{
	return 0;
}
#endif
