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

#include <stdlib.h>

#include <meta_html.h>

#include <meta_map.h>

/*
 * A template repository is a thread safe storage for html_templates.
 * An application can contain multiple templates and each thread need
 * some thread safe way to get hold of templates after they've been created.
 * We don't want global variables, but implement a singleton repository
 * instead.
 */
struct html_template_repository {
	map templates;
};

static struct html_template_repository* repos = NULL;


void html_template_repository_empty(void)
{
	if (repos != NULL) {
		map_free(repos->templates);
		free(repos);
		repos = NULL;
	}
}

static int init_repos(void)
{
	if (repos == NULL) {
		if ((repos = malloc(sizeof *repos)) == NULL)
			return 0;
		else if((repos->templates = map_new((void(*)(void*))html_template_free)) == NULL) {
			free(repos);
			repos = NULL;
			return 0;
		}
	}

	return 1;
}

html_buffer html_template_repository_use(const char* template)
{
	html_template t;
	html_buffer b;

	if (!init_repos())
		return NULL;

	if ((t = map_get(repos->templates, template)) == NULL)
		return NULL;
	else if((b = html_buffer_new()) == NULL)
		return NULL;
	else {
		html_buffer_set_template(b, t);
		return b;
	}
}

int html_template_repository_add(const char* name, html_template t)
{
	if (!init_repos())
		return 0;
	else if(!map_set(repos->templates, name, t))
		return 0;
	else
		return 1;
}
