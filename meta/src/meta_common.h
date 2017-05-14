/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#ifndef META_COMMON_H
#define META_COMMON_H

#include <assert.h>
#include <stdarg.h> /* for va_list */
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* stuff common to all modules.	 */

/*
 * A destructor (dtor) is a pointer to a function which frees
 * an object and all memory allocated by that object. We use dtor's
 * quite a lot, e.g. to destroy list members, array members and pool members.
 */
typedef void(*dtor)(void*);

/* We always need to be able to trace stuff, mainly for debugging.
 * Even if we think that everything is working fine, shit happens.
 * We therefore trace all functions called, including tid (if any)
 * We always support the following global variables:
 * int meta_verbose_level. 0 means no output
 * int meta_indent_level. For output
 * These variables are always present so that command line processing
 * can be simpler. They just don't do anything in release builds (NDEBUG).
 */
extern int meta_verbose_level;
extern int meta_indent_level;

void verbose(int level, const char *fmt, ...)
    __attribute__((format(printf,2,3)));

/* Write a warning to the syslog */
void warning(const char *fmt, ...);
void meta_vsyslog(int class, const char *fmt, va_list ap);

void warning(const char *fmt, ...)
    __attribute__((nonnull(1)))
    __attribute__((format(printf,1,2)));

void die(const char *fmt, ...)
    __attribute__((nonnull(1)))
    __attribute__((format(printf,1,2)))
    __attribute__ ((noreturn)) ;

void die_perror(const char *fmt, ...)
    __attribute__((nonnull(1)))
    __attribute__((format(printf,1,2)))
    __attribute__ ((noreturn));


/*
 * Debugging: Do we need it? Well, it's handy, at least in debug
 * builds. Let's utilize vararg macros and strip debug info from
 * non-debug builds. For security reasons, all release builds must
 * be stripped of both debugging functions and textual information.
 */
#ifdef NDEBUG
#define debug(...)

#else
#define debug(...) debugimpl(__VA_ARGS__)
#endif

// We always provide these, in case one compiles with debug info
// and links with the release version. It's not the library that's an
// issue, but the calling code. We can't control the callers anyway,
// but give them a chance to strip away debug info as long as they
// use the debug() macro. We can't control the callers anyway,
// but give them a chance to strip away debug info as long as they
// use the debug() macro.
void debugimpl(const char *fmt, ...);
void meta_enable_debug_output(void);
void meta_disable_debug_output(void);


/*
 * We want a distinct type to indicate success or failure,
 * instead of fiddling with true/false, 1/0, 0/-1 or 
 * other constructs. Therefoer we create a silly but useful type
 * named status_t, which is a pointer to a metastatus struct.
 * Since pointers are type-safe and cannot be implicitly converted
 * to integers, it's harder to mix status types(return values)
 * with integers. That's our goal too. We do NOT want to use
 * true/false/stdbool.h because the standard bool type mixes too easily
 * with int.
 * 20141211 boa
 */
typedef struct metastatus *status_t;
#define success ((status_t)1)
#define failure ((status_t)0)

/*
 * We use the errno variable to set/get errors, since it's so commonly used.
 * To simplify code, we throw in a silly function which sets errno for us
 * and returns an error.
 *
 * Note that we do NOT accept 0 as error cause. That'd clear errno and
 * return failure. We could of course add an if-test and return either
 * success or failure, but that'd add cycles and be semantically silly.
 *
 * 20160213 boa
 */
static inline status_t __attribute__((warn_unused_result)) fail(int cause)
{
    assert(cause != 0);

    errno = cause;
    return failure;
}


#ifdef __cplusplus
}
#endif

#endif
