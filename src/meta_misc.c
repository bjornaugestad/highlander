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
#include <syslog.h>
#include <pthread.h>

#include <meta_misc.h>

/**
 * Aix has no vsyslog, so we use our own.
 * We limit the output to 1000 characters. That should be
 * sufficient for most error messages.
 */
void meta_vsyslog(int class, const char* fmt, va_list ap)
{
	char err[1000];

	vsnprintf(err, sizeof err, fmt, ap);
	syslog(class, "%s", err);
}

int casecompstr(const char* s1, const char* s2)
{
	assert(s1 != NULL);
	assert(s2 != NULL);

	while (*s1 != '\0') {
		if (*s2 == '\0')
			return 1;	/* s1 is greater as it is longer */
		else if(*s1 != *s2)
			return *s1 - *s2;
		else {
			s1++;
			s2++;
		}
	}

	/* We reached the end of s1, but s2 may still have data.
	 * s2 may also be exausted, so the normal *s1 - *s2 will
	 * work perfectly.
	 */
	return *s1 - *s2;
}

void fs_lower(char* s)
{
	assert(NULL != s);

	while (*s != '\0') {
		if (isupper((int)*s))
			*s = tolower((int)*s);

		s++;
	}
}
void fs_upper(char* s)
{
	assert(NULL != s);

	while (*s != '\0') {
		if (islower((int)*s))
			*s = toupper((int)*s);

		s++;
	}
}

/* Returns -1 on error, else the number */
long fs_atol(const char* s)
{
	long val = 0;
	assert(s != NULL);

	while (*s) {
		if (isdigit((int)*s))
			val = (val * 10) + (*s - '0');
		else
			return -1;

		s++;
	}

	return val;
}

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

int find_word(const char* s, size_t iWord)
{
	const char* string = s;

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
	const char* string,
	char word[],
	size_t cchWordMax,	/* Max # of chars excl. '\0' in word */
	size_t iWord)		/* zero-based index of word to copy */
{
	int i;

	assert(string != NULL);
	assert(word != NULL);
	assert(cchWordMax > 1);

	i = find_word(string, iWord);

	/* if index out of range */
	if (i == -1)
		return 0;

	/* copy the word */
	string += i;
	return copy_word(string, word, ' ', cchWordMax);
}

int copy_word(
	const char* input,
	char word[],
	int separator,
	size_t cchWordMax)
{
	size_t i = 0;

	assert(input != NULL);
	assert(word != NULL);
	assert(separator != '\0');
	assert(cchWordMax > 0);

	while (*input != '\0' && *input != separator && i < cchWordMax)
		word[i++] = *input++;

	if (i == cchWordMax)
		return 0;

	word[i] = '\0';
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

void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	meta_vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void die_perror(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s", strerror(errno));
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

void warning(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	meta_vsyslog(LOG_WARNING, fmt, ap);
	va_end(ap);
}

int tprintf(FILE *f, int tabs, const char *format, ...)
{
	va_list ap;
	int rc;

	while (tabs--)
		fputc('\t', f);

	va_start(ap, format);
	rc = vfprintf(f, format, ap);
	va_end(ap);

	return rc;
}

int get_extension(const char* src, char* dest, size_t cb)
{
	const char* end;
	size_t i = cb - 1;
	int found = 0;

	assert(src != NULL);
	assert(dest != NULL);
	assert(cb > 1);

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
	else if(found) {
		memmove(dest, &dest[i + 1], cb - i);
	}
	else
		*dest = '\0';

	return 1;
}

int get_basename(const char* name, const char* suffix, char* dest, size_t cb)
{
	char* s;
	size_t i;

	assert(name != NULL);
	assert(dest != NULL);
	assert(cb > 1);

	/* Locate the rightmost / to remove the directory part */
	if ((s = strrchr(name, '/')) != NULL)
		s++;	/* Skip the slash */
	else
		s = (char*)name; /* The cast is OK. :-) */

	/* Now copy the filename part. */
	strncpy(dest, s, cb);
	dest[cb - 1] = '\0';

	/* Locate the suffix, if any */
	if (suffix == NULL || (s = strstr(dest, suffix)) == NULL)
		return 1;

	/* Be sure that the suffix actually is a suffix.
	 * We do not want to remove .tar.gz from foo.tar.gz
	 * if the suffix is .tar, so the suffix must be
	 * at the end of the string.
	 */
	i = strlen(suffix);
	if (s[i] == '\0')
		*s = '\0';

	return 1;
}


const char* get_inet_addr(void* paddr, char* dst, size_t cnt)
{
	char* pa;
	struct in_addr* pinaddr = paddr;

	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

	if (pthread_mutex_lock(&lock))
		return NULL;

	if ((pa = inet_ntoa(*pinaddr)) == NULL || strlen(pa) >= cnt) {
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	strcpy(dst, pa);

	pthread_mutex_unlock(&lock);
	return dst;
}

/**
 * This is probably bad, but the need for speed forces us to
 * do it this way. I cannot see any security issues unless the
 * user of Highlander actually wants it to fail, so here we go.
 * Decide the mime type for a file, based on extension.
 */

const char* get_mime_type(const char* filename)
{
	static struct {
		const char* ext;
		const char* mime;
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
	{ /* Check get_basename */
		char sz[1024] = { '\0' };
		if (!get_basename("foo.bar", ".bar", sz, sizeof sz)
		|| strcmp(sz, "foo") != 0) {
			fprintf(stderr, "get_basename failed: %s\n", sz);
			return EXIT_FAILURE;
		}

		if (!get_basename("/foo.bar", NULL, sz, sizeof sz)
		|| strcmp(sz, "foo.bar") != 0) {
			fprintf(stderr, "get_basename failed(2)\n");
			return EXIT_FAILURE;
		}
		memset(sz, '\0', sizeof sz);
		if (!get_basename("/a/b/z/d/e/foo.bar", "bar", sz, sizeof sz)
		|| strcmp(sz, "foo.") != 0) {
			fprintf(stderr, "get_basename failed(3): %s\n", sz);
			return EXIT_FAILURE;
		}
	}


	return 0;
}
#endif

