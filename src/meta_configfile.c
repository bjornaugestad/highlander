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

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_configfile.h>

/**
 * Local helper struct storing name and value for a directive.
 */
struct nameval {
	char* name;
	char* value;
};

#define MAX_DIRECTIVES 2000

/**
 * We keep it simple and just allocate up to MAX_DIRECTIVES pointers
 * to nameval structs. Then we use these pointers and place
 * names and values in them.
 */
struct configfile_tag {
	struct nameval* values[MAX_DIRECTIVES];
	size_t used;
};

/**
 * Returns 0 if no name/value was found, 1 if it was and -1 if syntax
 * error. Note that we write to line.
 */
static int get_name_and_value(char *line, char *name, char *value)
{
	char *s, *org_name = name;
	int quoted = 0;

	/* Remove comments, if any */
	if( (s = strchr(line, '#')) != NULL)
		*s = '\0';

	/* Get first non-ws character */
	s = line;
	while(*s != '\0' && isspace((unsigned char)*s))
		s++;

	/* Was the line ws only? */
	if(*s == '\0') 
		return 0;

	/* Now s points to the first char in name.
	 * Copy to the param name */
	 while(*s != '\0' && !isspace((unsigned char)*s)) 
	 	*name++ = *s++;

	/* See if we found a space or not.
	 * The space(or tab) is required */
	if(*s == '\0')
		return -1;

	/* Terminate name and look for next non-ws
	 * which is the start of the value
	 */
	*name = '\0';
	while(*s != '\0' && isspace((unsigned char)*s))
		s++;

	/* Skip leading ", if any */
	if(*s == '"') {
		quoted = 1;
		s++;
	}

	/* Now copy value */
	if(quoted) {
		while(*s != '\0' && *s != '"')
			*value++ = *s++;
	}
	else {
		while(*s != '\0' && !isspace((unsigned char)*s))
			*value++ = *s++;
	}

	/* Terminate value and perform sanity check */
	*value = '\0';
	if(strlen(org_name) == 0)
		return -1;

	return 1;
}

static int add(configfile cf, const char* name, const char* value)
{
	char *n = NULL, *v = NULL;

	assert(cf != NULL);
	assert(name != NULL);
	assert(value != NULL);

	if(cf->used >= MAX_DIRECTIVES)
		return 0;

	if( (n = mem_malloc(strlen(name) + 1)) == NULL
	||  (v = mem_malloc(strlen(value) + 1)) == NULL
	||  (cf->values[cf->used] = mem_malloc(sizeof(struct nameval))) == NULL) {
		mem_free(n);
		mem_free(v);
		return 0;
	}

	strcpy(n, name);
	strcpy(v, value);
	cf->values[cf->used]->name = n;
	cf->values[cf->used]->value = v;
	cf->used++;
	return 1;
}


configfile configfile_read(const char *path)
{
	FILE *f;
	char line[2048];
	char name[2048];
	char value[2048];
	configfile p;
	
	if( (f = fopen(path, "r")) == NULL) 
		return NULL;

	if( (p = mem_calloc(1, sizeof *p)) == NULL) {
		fclose(f);
		return NULL;
	}

	p->used = 0;
	while(fgets(line, (int)sizeof(line), f)) {
		int i = get_name_and_value(line, name, value);
		if(i == -1) {
			fclose(f);
			configfile_free(p);
			errno = EINVAL;
			return NULL;
		}
		else if(i == 1) {
			if(!add(p, name, value)) {
				fclose(f);
				configfile_free(p);
				return NULL;
			}
		}
	}

	fclose(f);
	return p;
}

static struct nameval* find(configfile cf, const char* name)
{
	size_t i;

	assert(cf != NULL);
	assert(name != NULL);

	for(i = 0; i < cf->used; i++) {
		if(strcmp(name, cf->values[i]->name) == 0)
			return cf->values[i];
	}

	return NULL;
}

int configfile_exists(configfile cf, const char* name)
{
	assert(cf != NULL);
	assert(name != NULL);

	if(find(cf, name) != NULL)
		return 1;
	else
		return 0;
}

int configfile_get_string(configfile cf, const char *name, char *value, size_t cb)
{
	struct nameval* pnv;

	assert(cf != NULL);
	assert(name != NULL);
	assert(value != NULL);
	assert(cb > 1);

	if( (pnv = find(cf, name)) == NULL) {
		errno = ENOENT;
		return 0;
	}
	else if(strlen(pnv->value) + 1 > cb) {
		errno = ENOSPC;
		return 0;
	}
	else {
		strcpy(value, pnv->value);
		return 1;
	}
}

int configfile_get_long(configfile cf, const char *name, long *value)
{
	char sz[20];

	assert(cf != NULL);
	assert(name != NULL);
	assert(value != NULL);

	if(!configfile_get_string(cf, name, sz, sizeof sz))
		return 0;
	
	errno = 0;
	*value = strtol(sz, NULL, 10);
	if(*value == LONG_MIN || *value == LONG_MAX) 
		return 0;
	else if(*value == 0 && errno == EINVAL)
		return 0;
	else
		return 1;
}

int configfile_get_ulong(configfile cf, const char *name, unsigned long *value)
{
	char sz[20];

	assert(cf != NULL);
	assert(name != NULL);
	assert(value != NULL);

	if(!configfile_get_string(cf, name, sz, sizeof sz))
		return 0;
	
	errno = 0;
	*value = strtol(sz, NULL, 10);
	if(*value == ULONG_MAX) 
		return 0;
	else if(*value == 0 && errno == EINVAL)
		return 0;
	else
		return 1;
}


int configfile_get_uint(configfile cf, const char *name, unsigned int *value)
{
	unsigned long tmp;

	assert(cf != NULL);
	assert(name != NULL);
	assert(value != NULL);

	if(!configfile_get_ulong(cf, name, &tmp))
		return 0;
	
	if(tmp > UINT_MAX) {
		errno = EINVAL;
		return 0;
	}
	else {
		*value = (unsigned int)tmp;
		return 1;
	}
}
int configfile_get_int(configfile cf, const char *name, int *value)
{
	long tmp;

	assert(cf != NULL);
	assert(name != NULL);
	assert(value != NULL);

	if(!configfile_get_long(cf, name, &tmp))
		return 0;
	
	if(tmp < INT_MIN || tmp > INT_MAX) {
		errno = EINVAL;
		return 0;
	}
	else {
		*value = (int)tmp;
		return 1;
	}
}

void configfile_free(configfile cf)
{
	if(cf != NULL) {
		size_t i;

		for(i = 0; i < cf->used; i++) {
			mem_free(cf->values[i]->name);
			mem_free(cf->values[i]->value);
			mem_free(cf->values[i]);
		}

		mem_free(cf);
	}
}

#ifdef CHECK_CONFIGFILE
int main(void)
{
	const char* filename = "./configfile.conf";
	char string[1024];
	int nint; long nlong;
	const char* quotedstring = "this is a quoted string";

	configfile cf;


	if( (cf = configfile_read(filename)) == NULL)
		return 77;

	if(!configfile_get_string(cf, "logrotate", string, sizeof(string)))
		return 77;

	if(!configfile_get_int(cf, "logrotate", &nint))
		return 77;

	if(!configfile_get_long(cf, "logrotate", &nlong))
		return 77;

	if(!configfile_get_string(cf, "quotedstring", string, sizeof(string)))
		return 77;

	if(strcmp(string, quotedstring)) {
		fprintf(stderr, "Expected %s, got %s\n", quotedstring, string);
		return 77;
	}

	configfile_free(cf);
	return 0;
}
#endif

