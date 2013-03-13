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
#include <string.h>
#include <assert.h>

#include <cstring.h>
#include <meta_html.h>

#include <meta_list.h>

/* A section is stored using this structure */
struct html_section_tag {
	char* name;
	char* code;
};

html_section html_section_new(void)
{
	html_section p;

	if( (p = malloc(sizeof *p)) == NULL) 
		;
	else {
		p->name = NULL;
		p->code = NULL;
	}

	return p;
}

void html_section_free(html_section p)
{
	if(p != NULL) {
		free(p->name);
		free(p->code);
		free(p);
	}
}

int html_section_set_name(html_section s, const char* v)
{
	assert(s != NULL);
	assert(v != NULL);

	if(s->name != NULL) 
		free(s->name);

	if( (s->name = malloc(strlen(v) + 1)) != NULL)
		strcpy(s->name, v);

	return s->name != NULL;
}

int html_section_set_code(html_section s, const char* v)
{
	assert(s != NULL);
	assert(v != NULL);

	if(s->code != NULL) 
		free(s->code);

	if( (s->code = malloc(strlen(v) + 1)) != NULL)
		strcpy(s->code, v);

	return s->code != NULL;
}

const char* html_section_get_code(html_section s)
{
	assert(s != NULL);
	return s->code;
}

const char* html_section_get_name(html_section s)
{
	assert(s != NULL);
	return s->name;
}

