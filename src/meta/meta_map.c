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
#include <string.h>

#include <meta_list.h>
#include <meta_map.h>

/*
 * We implement the map as a list containing pairs. The pair
 * is private to this file size we have another class named pair.
 * (That other class should really be renamed one day)
 */
struct pair {
	char *key;
	void *value;
};

/*
 * Wrap the list to provide type safety
 */
struct map_tag {
	list a;
	void(*freefunc)(void* arg);
};

map_iterator map_first(map m)
{
	map_iterator mi;
	list_iterator li;

	li = list_first(m->a);
	mi.node = li.node;
	return mi;
}

map_iterator map_next(map_iterator mi)
{
	list_iterator li;

	li.node = mi.node;
	li = list_next(li);
	mi.node = li.node;
	return mi;
}

int map_end(map_iterator mi)
{
	list_iterator li;

	li.node = mi.node;
	return list_end(li);
}

char* map_key(map_iterator mi)
{
	struct pair *p;
	list_iterator li;

	li.node = mi.node;
	p = list_get(li);
	return p->key;
}

void* map_value(map_iterator mi)
{
	struct pair *p;
	list_iterator li;

	li.node = mi.node;
	p = list_get(li);
	return p->value;
}

map map_new(void(*freefunc)(void* arg))
{
	map m;

	if ((m = calloc(1, sizeof *m)) == NULL)
		;
	else if ((m->a = list_new()) == NULL) {
		free(m);
		m = NULL;
	}
	else
		m->freefunc = freefunc;

	return m;
}

void map_free(map m)
{
	list_iterator i;
	struct pair *p;

	if (m != NULL && m->a != NULL) {
		for (i = list_first(m->a); !list_end(i); i = list_next(i)) {
			p = list_get(i);
			free(p->key);
			if (m->freefunc != NULL)
				m->freefunc(p->value);
		}
	}

	list_free(m->a, NULL);
	free(m);
}

static struct pair*
map_find(map m, const char* key)
{
	list_iterator i;

	for (i = list_first(m->a); !list_end(i); i = list_next(i)) {
		struct pair *p;

		p = list_get(i);

		if (strcmp(key, p->key) == 0)
			return p;
	}

	return NULL;
}

int map_set(map m, const char* key, void* value)
{
	struct pair *p, *i;


	if ((i = map_find(m, key)) != NULL) {
		free(i->value);
		i->value = value;

		return 0;
	}
	else if ((p = malloc(sizeof *p)) == NULL)
		return 0;
	else if ((p->key = malloc(strlen(key) + 1)) == NULL)  {
		free(p);
		return 0;
	}
	else {
		strcpy(p->key, key);
		p->value = value;
		return list_add(m->a, p) != NULL;
	}
}

int map_exists(map m, const char* key)
{
	if (map_find(m, key))
		return 1;
	else
		return 0;
}

void* map_get(map m, const char* key)
{
	struct pair *p;

	if ((p = map_find(m, key)) != NULL)
		return p->value;
	else
		return NULL;
}

int map_foreach(map m, void* args, int(*f)(void* args, char* key, void* data))
{
	list_iterator i;

	for (i = list_first(m->a); !list_end(i); i = list_next(i)) {
		struct pair *p;

		p = list_get(i);
		if (f(args, p->key, p->value) == 0)
			return 0;
	}

	return 1;
}
