/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_MISC_H
#define META_MISC_H

#include <stddef.h>		/* for size_t */

#include <meta_common.h>

#ifdef __cplusplus
extern "C" {
#endif

void fs_lower(char *s);

void remove_trailing_newline(char *s);

void trim(char *s);
void ltrim(char *s);
void rtrim(char *s);

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
 *	find_word("foo bar", 0)		returns 0, as f in foo is at offset 0
 *	find_word(" foo bar", 0)	returns 1
 *	find_word("foo bar", 1)		returns 4, as b in bar is at offset 4.
 *	find_word("foo bar", 2)		returns -1, as the wordidx is zero-based and 2 is out of range
 *	find_word("foo bar fly", 2)	returns 8, as f in fly is at offset 8.
 */
int find_word(const char *s, size_t wordidx);

/*
 * Extract the extension from a filename. Returns
 * 1 if successful. Note that 1 will be returned even if
 * no extension exists as it is legal not to have an extension.
 */
status_t get_extension(const char *src, char *dest, size_t destsize);
const char *get_mime_type(const char *filename);

/*
 * Copies one space-separated word from string.
 * Returns 0 on error and 1 on success
 * @param destsize	Max # of chars excl. '\0' in word
 * @param wordidx			 zero-based index of word to copy
 */
status_t get_word_from_string(const char *src, char *dest,
    size_t destsize, size_t wordidx)
    __attribute__((warn_unused_result));

/*
 * Copies one word from 'src' and places it in 'dest'.
 * Stops at either '\0' or 'separator'.
 * Terminates 'dest' with a '\0' if enough space was available.
 */
status_t copy_word(const char *src, char *dest, int separator,
    size_t destsize)
    __attribute__((warn_unused_result));

#if !defined(min) && !defined(max)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef __cplusplus
}
#endif

#endif
