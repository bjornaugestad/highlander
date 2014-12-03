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

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>

#include <meta_common.h>
#include <cstring.h>

#define CSTRING_INITIAL_SIZE 256

int cstring_extend(cstring s, size_t size)
{
	size_t bytes_needed, newsize;
	char *data;

	assert(s != NULL);
	assert(s->data != NULL);
	assert(s->size >= CSTRING_INITIAL_SIZE);
	assert(s->len < s->size);

	/* Check for available space and reallocate if necessary */
	bytes_needed = s->len + size + 1;
	if (bytes_needed > s->size) {
		/* Double the size of the buffer */
		newsize = s->size * 2;
		if (newsize < bytes_needed) {
			/* Doubling wasn't sufficient */
			newsize = s->size + size;
		}

		if ((data = realloc(s->data, newsize)) == NULL)
			return 0;
		else {
			s->data = data;
			s->size = newsize;
		}
	}

	assert(s->len == strlen(s->data));
	return 1;
}

int cstring_vprintf(
	cstring dest,
	size_t needs_max,
	const char *fmt,
	va_list ap)
{
	int i;

	/* Adjust the size of needs_max. vsnprintf includes the '\0'
	 * in its computation, so vprintf(,,,strlen(x), "%s", x) fails.
	 */
	needs_max++;

	assert(NULL != dest);
	assert(NULL != dest->data);
	assert(dest->len == strlen(dest->data));

	if (!cstring_extend(dest, needs_max))
		return 0;

	/* We append the new data, therefore the & */
	i = vsnprintf(&dest->data[dest->len], needs_max, fmt, ap);

	/* We do not know the length of the data after vsnprintf()
	 * We therefore recompute the len member.
	 */
	dest->len += i;

	assert(dest->len == strlen(dest->data));
	return 1;
}

int cstring_printf(cstring dest, size_t needs_max, const char *fmt, ...)
{
	int success;
	va_list ap;

	assert(NULL != dest);
	assert(NULL != fmt);

	va_start(ap, fmt);
	success = cstring_vprintf(dest, needs_max, fmt, ap);
	va_end(ap);

	return success;
}

int cstring_pcat(cstring dest, const char *start, const char *end)
{
	ptrdiff_t cb;

	assert(dest != NULL);
	assert(start != NULL);
	assert(end != NULL);
	assert(start < end);

	cb = end - start;
	if (!cstring_extend(dest, cb))
		return 0;

	memcpy(&dest->data[dest->len], start, cb);
	dest->len += cb;
	dest->data[dest->len] = '\0';

	assert(dest->len == strlen(dest->data));
	return 1;
}

int cstring_concat(cstring dest, const char *src)
{
	size_t cb;

	assert(NULL != src);
	assert(NULL != dest);

	cb = strlen(src);
	if (!cstring_extend(dest, cb))
		return 0;

	/* Now add the string to the dest */
	strcat(&dest->data[dest->len], src);
	dest->len += cb;

	assert(dest->len == strlen(dest->data));
	return 1;
}

int cstring_charcat(cstring dest, int c)
{
	assert(NULL != dest);

	/* This function gets called a lot these days, esp.
	 * after the html_template ADT was added. I've
	 * therefore added a small extra test here to avoid
	 * millions of function calls.
	 */
	if (dest->len >= dest->size) {
		if (!cstring_extend(dest, 1))
			return 0;
	}

	dest->data[dest->len++] = c;
	dest->data[dest->len] = '\0';

	assert(dest->len == strlen(dest->data));
	return 1;
}

cstring cstring_new(void)
{
	cstring p;

	/*
	 * Rumours are that some OS'es use an optimistic memory allocation
	 * strategy, which means that they don't allocate memory until the
	 * mem is accessed. We therefore request memory with calloc
	 * instead of malloc.
	 * Other rumours say that the OS just marks the page as a zeroed
	 * page and doesn't do anything until the page is read from.
	 * Hmm, what's a poor coder to do? Write to the page? That hurts
	 * performance...
	 */
	if ((p = calloc(1, sizeof *p)) == NULL)
		;
	else if ((p->data = calloc(1, CSTRING_INITIAL_SIZE)) == NULL) {
		free(p);
		p = NULL;
	}
	else {
		p->len = 0;
		p->size = CSTRING_INITIAL_SIZE;
		*p->data = '\0';
	}

	return p;
}

cstring cstring_dup(const char *src)
{
	cstring dest = NULL;

	assert(src != NULL);
	if ((dest = cstring_new()) != NULL) {
		if (!cstring_copy(dest, src)) {
			cstring_free(dest);
			dest = NULL;
		}
	}

	return dest;
}

int cstring_copy(cstring dest, const char *src)
{
	size_t c;

	assert(NULL != dest);
	assert(NULL != dest->data);
	assert(NULL != src);

	cstring_recycle(dest);
	c = strlen(src);
	if (!cstring_extend(dest, c))
		return 0;

	strcpy(dest->data, src);
	dest->len += c;

	assert(dest->len == strlen(dest->data));
	return 1;
}

int cstring_ncopy(cstring dest, const char *src, const size_t cch)
{
	size_t c;

	assert(NULL != dest);
	assert(NULL != dest->data);
	assert(NULL != src);

	cstring_recycle(dest);
	c = strlen(src);
	if (c > cch)
		c = cch;

	if (!cstring_extend(dest, c))
		return 0;

	strncpy(dest->data, src, c);
	dest->data[c] = '\0';
	dest->len = c;

	assert(dest->len == strlen(dest->data));
	return 1;
}

int cstring_concat2(cstring dest, const char *s1, const char *s2)
{
	assert(NULL != dest);
	assert(NULL != s1);
	assert(NULL != s2);

	if (cstring_concat(dest, s1) && cstring_concat(dest, s2))
		return 1;
	else
		return 0;
}

int cstring_concat3(
	cstring dest,
	const char *s1,
	const char *s2,
	const char *s3)
{
	assert(NULL != dest);
	assert(NULL != s1);
	assert(NULL != s2);
	assert(NULL != s3);

	if (cstring_concat(dest, s1)
	&& cstring_concat(dest, s2)
	&& cstring_concat(dest, s3))
		return 1;
	else
		return 0;
}

int cstring_multinew(cstring* pstr, size_t nelem)
{
	size_t i;

	assert(pstr != NULL);
	assert(nelem > 0);

	for (i = 0; i < nelem; i++) {
		if ((pstr[i] = cstring_new()) == NULL) {
			while (i-- > 0)
				cstring_free(pstr[i]);

			return 0;
		}
	}

	return 1;
}

/* Free multiple cstrings with one call */
void cstring_multifree(cstring *pstr, size_t nelem)
{
	size_t i;

	assert(pstr != NULL);

	for (i = 0; i < nelem; i++)
		cstring_free(pstr[i]);
}

cstring cstring_left(cstring src, size_t n)
{
	cstring dest;

	assert(src != NULL);

	if ((dest = cstring_new()) == NULL || !cstring_extend(dest, n)) {
		cstring_free(dest);
		dest = NULL;
	}
	else
		cstring_ncopy(dest, src->data, n);

	return dest;
}

cstring cstring_right(cstring src, size_t n)
{
	cstring dest;

	/* Get mem */
	if ((dest = cstring_new()) == NULL || !cstring_extend(dest, n)) {
		cstring_free(dest);
		dest = NULL;
	}
	else {
		const char *s = src->data;
		size_t cb = strlen(s);

		/* Copy string */
		if (cb > n)
			s += cb - n;
		cstring_copy(dest, s);
	}

	return dest;
}

cstring cstring_substring(cstring src, size_t from, size_t to)
{
	cstring dest;
	size_t cb;

	assert(src != NULL);
	assert(from <= to);
	assert(to <= src->len);

	if (to > src->len)
		to = src->len;

	cb = to - from + 1;
	if ((dest = cstring_new()) == NULL || !cstring_extend(dest, cb)) {
		cstring_free(dest);
		dest = NULL;
	}
	else
		cstring_pcat(dest, &src->data[from], &src->data[to]);

	return dest;
}

void cstring_reverse(cstring s)
{
	if (s->len > 0) {
		char *beg = s->data;
		char *end = s->data + s->len - 1;
		while (beg < end) {
			char tmp = *end;
			*end-- = *beg;
			*beg++ = tmp;
		}
	}
}


/*
 * 1. Count the number of words
 * 2. Allocate space for an array of cstrings.
 * 3. Allocate each cstring.
 * 4. Copy each word
 */
size_t cstring_split(cstring** dest, const char *src, const char *delim)
{
	const char *s;
	size_t end, i, len, nelem;

	assert(dest != NULL);
	assert(src != NULL);
	assert(delim != NULL);

	/* Skip to first substring */
	len = strlen(src);
	s = src + strspn(src, delim);

	/* Only delimiters in src */
	if (s - src == (int)len)
		return 0;

	/* Count elements */
	for (nelem = 0; *s != '\0'; nelem++) {
		s += strcspn(s, delim);
		s += strspn(s, delim);
	}

	/* allocate space */
	if ((*dest = malloc(sizeof *dest * nelem)) == NULL)
		return 0;
	else if (cstring_multinew(*dest, nelem) == 0) {
		free(*dest);
		return 0;
	}

	/* Now copy */
	s = src + strspn(src, delim); /* start of first substring */
	for (i = 0; *s != '\0'; i++) {
		end = strcspn(s, delim);
		if (!cstring_pcat((*dest)[i], s, s + end)) {
			cstring_multifree(*dest, nelem);
			return 0;
		}

		s += end;
		s += strspn(s, delim);
	}

	return nelem;
}

void cstring_strip(cstring s)
{
	size_t i;

	assert(s != NULL);

	/* strip trailing ws first */
	i = s->len;
	while (i-- > 0 && isspace((unsigned char)s->data[i])) {
		s->data[i] = '\0';
		s->len--;
	}

	/* Now leading ws */
	for (i = 0; i < s->len; i++) {
		if (!isspace((unsigned char)s->data[i]))
			break;
	}

	if (i > 0) {
		s->len -= i;
		memmove(&s->data[0], &s->data[i], s->len);
		s->data[s->len] = '\0';
	}

}

void cstring_lower(cstring s)
{
	size_t i;

	assert(s != NULL);

	for (i = 0; i < s->len; i++) {
		if (isupper((unsigned char)s->data[i]))
			s->data[i] = tolower((unsigned char)s->data[i]);
	}
}

void cstring_upper(cstring s)
{
	size_t i;

	assert(s != NULL);

	for (i = 0; i < s->len; i++) {
		if (islower((unsigned char)s->data[i]))
			s->data[i] = toupper((unsigned char)s->data[i]);
	}
}


#ifdef CHECK_CSTRING
int main(void)
{
	cstring s, dest, *pstr;
	int rc;
	const char *start = "This is a string";
	const char *end = start + strlen(start);

	char longstring[10000];

	size_t i, nelem = 100;

	memset(longstring, 'A', sizeof(longstring));
	longstring[sizeof(longstring) - 1] = '\0';


	for (i = 0; i < nelem; i++) {
		s = cstring_new();
		assert(s != NULL);

		rc = cstring_copy(s, "Hello");
		assert(rc == 1);

		rc = cstring_compare(s, "Hello");
		assert(rc == 0);

		rc = cstring_compare(s, "hello");
		assert(rc != 0);

		rc = cstring_concat(s, ", world");
		assert(rc == 1);

		rc = cstring_compare(s, "Hello, world");
		assert(rc == 0);

		rc = cstring_concat(s, longstring);
		assert(rc == 1);

		rc = cstring_charcat(s, 'A');
		assert(rc == 1);

		cstring_recycle(s);
		rc = cstring_concat(s, longstring);
		assert(rc == 1);

		rc = cstring_concat2(s, longstring, longstring);
		assert(rc == 1);

		rc = cstring_concat3(s, longstring, longstring, longstring);
		assert(rc == 1);

		/* Test strpcat */
		cstring_recycle(s);
		rc = cstring_pcat(s, start, end);
		assert(rc == 1);

		rc = cstring_compare(s, start);
		assert(rc == 0);

		/* Test cstring_left() */
		cstring_copy(s, "hello, world");
		dest = cstring_left(s, 5);
		rc = cstring_compare(dest, "hello");
		assert(rc == 0);
		cstring_free(dest);

		/* Test cstring_left() with short strings */
		dest = cstring_left(s, 5000);
		rc = cstring_compare(dest, "hello, world");
		assert(rc == 0);
		cstring_free(dest);

		/* cstring_right() */
		cstring_copy(s, "hello, world");
		dest = cstring_right(s, 5);
		rc = cstring_compare(dest, "world");
		assert(rc == 0);
		cstring_free(dest);

		dest = cstring_right(s, 5000);
		rc = cstring_compare(dest, "hello, world");
		assert(rc == 0);
		cstring_free(dest);

		/* cstring_substring */
		cstring_copy(s, "hello, world");
		dest = cstring_substring(s, 0, 5);
		rc = cstring_compare(dest, "hello");
		assert(rc == 0);
		cstring_free(dest);

		dest = cstring_substring(s, 1, 5);
		rc = cstring_compare(dest, "ello");
		assert(rc == 0);
		cstring_free(dest);

		dest = cstring_substring(s, 7, 12);
		rc = cstring_compare(dest, "world");
		assert(rc == 0);
		cstring_free(dest);

		/* cstring_reverse */
		cstring_copy(s, "hello, world");
		cstring_reverse(s);
		rc = cstring_compare(s, "dlrow ,olleh");
		assert(rc == 0);
		/* cstring_strip */

		cstring_copy(s, "  a b c d e f	");
		cstring_strip(s);
		rc = cstring_compare(s, "a b c d e f");
		assert(rc == 0);

		cstring_upper(s);
		rc = cstring_compare(s, "A B C D E F");
		assert(rc == 0);

		cstring_lower(s);
		rc = cstring_compare(s, "a b c d e f");
		assert(rc == 0);

		cstring_free(s);

		/* cstring_split() */
		rc = cstring_split(&pstr, "foo bar baz", " ");
		assert(rc == 3);
		cstring_multifree(pstr, rc);
		free(pstr);


		rc = cstring_split(&pstr, "          foo bar baz", " ");
		assert(rc == 3);
		cstring_multifree(pstr, rc);
		free(pstr);

		rc = cstring_split(&pstr, "       foo bar baz      ", " ");
		assert(rc == 3);
		cstring_multifree(pstr, rc);
		free(pstr);

		rc = cstring_split(&pstr, "       foo ", " ");
		assert(rc == 1);
		cstring_multifree(pstr, rc);
		free(pstr);


	}

	return 0;
}
#endif
