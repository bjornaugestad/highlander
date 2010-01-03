#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <meta_common.h>

static void(*g_errhandler)(void) = NULL;
static int g_policy = MEM_POLICY_NONE;

static void handle_error(void)
{
	if(g_errhandler != NULL)
		g_errhandler();

	if(g_policy == MEM_POLICY_ABORT)
		abort();
}

void *mem_malloc(size_t cb)
{
	assert(cb > 0); /* portability issues, even if it is legal C */
	void *p;

	if( (p = malloc(cb)) == NULL)
		handle_error();

	return p;
}

void *mem_calloc(size_t nelem, size_t size)
{
	void *p;

	assert(nelem > 0); /* portability issues, even if it is legal C */
	assert(size > 0); /* portability issues, even if it is legal C */

	if( (p = calloc(nelem, size)) == NULL)
		handle_error();

	return p;
}

void *mem_realloc(void* ptr, size_t size)
{
	void *p;

	/* Do not allow realloc() to return memory of 0 bytes */
	assert(ptr != NULL || size > 0);

	if( (p = realloc(ptr, size)) == NULL)
		handle_error();

	return p;
}

char* mem_strdup(const char* src)
{
	char* dest;

	if( (dest = malloc(strlen(src) + 1)) == NULL)
		handle_error();

	strcpy(dest, src);
	return dest;
}

void mem_free(void *p)
{
	free(p);
}

void  mem_set_error_handler(void(*handler)(void))
{
	g_errhandler = handler;
}

void  mem_set_error_policy(int policy)
{
	g_policy = policy;
}

int meta_verbose_level = 0;
int meta_indent_level = 0;

static void indent(int levels)
{
	while(levels-- > 0)
		putchar('\t');
}

void verbose(int level, const char *fmt, ...)
{
	if(level <= meta_verbose_level) {
		indent(meta_indent_level);
		va_list ap;
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}
