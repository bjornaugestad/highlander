/*
    libcoremeta - A library of useful C functions and ADT's
    Copyright (C) 2000-2006 B. Augestad, bjorn.augestad@gmail.com 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef MISCFUNC_H__
#define MISCFUNC_H__

#include <stddef.h> 	/* for size_t */
#include <stdarg.h> /* for va_list */
#include <stdio.h> 	/* for FILE* */

#ifdef __cplusplus
extern "C" {
#endif

void fs_lower(char* s);
void fs_upper(char* s);
long fs_atol(const char* s);

int tprintf(FILE *f, int tabs, const char *format, ...);
/**
 * Converts a string of digits only to size_t.
 * returns 0 on error, 1 on success.
 */
int string2size_t(const char *s, size_t *val);

void remove_trailing_newline(char *s);

/**
 * Replaces strcasecomp(), must be identical in use 
 * Probably a bit slow, as it is plain C.
 * Ignores LOCALE
 */
int casecompstr(const char* s1, const char* s2);
int get_basename(const char* name, const char* suffix, char* dest, size_t cb);

/**
 * Returns the # of words, separated by space, in the string.
 * "foo" is 1
 * "foo bar" is 2
 * " foo     bar        " is 2 as well
 * Note that this function is for alnums and space only.
 * It counts e.g. \t and \n as words.
 */
int get_word_count(const char *s);

/**
 * Returns an index to the first char in a word within the string.
 * returns -1 if index is invalid.
 * Example:
 * 	find_word("foo bar", 0)		returns 0
 * 	find_word(" foo bar", 0)	returns 1
 * 	find_word("foo bar", 1)		returns 4
 * 	find_word("foo bar", 2)		returns -1
 * 	find_word("foo bar fly", 2)	returns 8
 */
int find_word(const char* s, size_t iWord);

/**
 * Extract the extension from a filename. Returns 
 * 1 if successful. Note that 1 will be returned even if
 * no extension exists as it is legal not to have an extension.
 */
int get_extension(const char* src, char* dest, size_t cb);
const char* get_mime_type(const char* filename);

/**
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

/**
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
void warning(const char* fmt, ...);
void meta_vsyslog(int class, const char* fmt, va_list ap);

/* Assert for doubles. Double values tend to be almost equal, this macro
 * and function tests for that, allowing some slack.
 */
#ifndef NDEBUG
#define dassert(a, b) dassert_impl(__FILE__, __LINE__, #a, a, b)
void dassert_impl(const char* file, int line, const char* expr, double a, double b);
#else
#define dassert(a, b) (void)0
#endif

#if !defined(min) && !defined(max)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef __cplusplus
}
#endif

void die(const char *fmt, ...) __attribute__ ((noreturn));
void die_perror(const char *fmt, ...);

/**
 * Some platforms do not support inet_ntop. The normal replacement function
 * is inet_ntoa, but that function is not thread safe. We therefore provide
 * our own thread safe version of inet_ntop on platforms without that function.
 * The paddr argument must point to a struct in_addr.
 */
const char* get_inet_addr(void* paddr, char* dst, size_t cnt);

#endif /* guard */

