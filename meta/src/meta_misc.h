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

#ifndef META_MISC_H
#define META_MISC_H

#include <stddef.h>		/* for size_t */
#include <stdarg.h> /* for va_list */

#ifdef __cplusplus
extern "C" {
#endif

void fs_lower(char *s);

/*
 * Converts a string of digits only to size_t.
 * returns 0 on error, 1 on success.
 */
int string2size_t(const char *s, size_t *val);

void remove_trailing_newline(char *s);

/*
 * Returns the # of words, separated by space, in the string.
 * "foo" is 1
 * "foo bar" is 2
 * " foo	 bar		" is 2 as well
 * Note that this function is for alnums and space only.
 * It counts e.g. \t and \n as words.
 */
int get_word_count(const char *s);

/*
 * Returns an index to the first char in a word within the string.
 * returns -1 if index is invalid.
 * Example:
 *	find_word("foo bar", 0)		returns 0
 *	find_word(" foo bar", 0)	returns 1
 *	find_word("foo bar", 1)		returns 4
 *	find_word("foo bar", 2)		returns -1
 *	find_word("foo bar fly", 2)	returns 8
 */
int find_word(const char *s, size_t iWord);

/*
 * Extract the extension from a filename. Returns
 * 1 if successful. Note that 1 will be returned even if
 * no extension exists as it is legal not to have an extension.
 */
int get_extension(const char *src, char *dest, size_t destsize);
const char *get_mime_type(const char *filename);

/*
 * Copies one space-separated word from string.
 * Returns 0 on error and 1 on success
 * @param cchWordMax	Max # of chars excl. '\0' in word
 * @param iWord			 zero-based index of word to copy
 */
int get_word_from_string(
	const char *string,
	char word[],
	size_t cchWordMax,
	size_t iWord);

/*
 * Copies one word from 'input' and places it in 'word'.
 * Stops at either '\0' or 'separator'.
 * Terminates 'word' with a '\0' if enough space was available.
 * Returns -1 on buffer overflow and 0 on success.
 */
int copy_word(
	const char *input,
	char word[],
	int separator,
	size_t cchWordMax);

/* Write a warning to the syslog */
void warning(const char *fmt, ...);
void meta_vsyslog(int class, const char *fmt, va_list ap);

void warning(const char *fmt, ...)
	__attribute__((format(printf,1,2)));

void die(const char *fmt, ...)
	__attribute__((format(printf,1,2)))
	__attribute__ ((noreturn)) ;

void die_perror(const char *fmt, ...)
	__attribute__((format(printf,1,2)))
	__attribute__ ((noreturn));


#if !defined(min) && !defined(max)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef __cplusplus
}
#endif

#endif
