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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <meta_list.h>
#include <meta_stringmap.h>

/* Hash-algoritmen er en variant av dbj2, som er slik: */
static inline unsigned long hash(const unsigned char *str)
{
	unsigned long hashval = 5381;
	int c;

	while ((c = *str++) != '\0')
		hashval = ((hashval << 5) + hashval) + c;

	return hashval;
}

/* This is what we store in the hash table.
 * s is the original string, hashval the computed hash value
 * and id is the unique id of the entry.
 */
struct entry {
	char *s;
	unsigned long hashval;
	unsigned long id;
};

static void entry_free(void *pe)
{
	struct entry* p = pe;
	assert(p != NULL);
	assert(p->s != NULL);
	free(p->s);
	free(p);
}

/*
 * Implementation of the stringmap ADT
 */
struct stringmap_tag {
	size_t nelem;
	list* hashtable;
	unsigned long last_id;
};

int stringmap_foreach(stringmap sm, int(*fn)(const char*s, void *arg), void *arg)
{
	int rc;
	size_t i;

	assert(sm != NULL);
	assert(arg != NULL);

	for (i = 0; i < sm->nelem; i++) {
		if (sm->hashtable[i] != NULL) {
			list lst = sm->hashtable[i];
			list_iterator li;

			for (li = list_first(lst); !list_end(li); li = list_next(li)) {
				struct entry* p =  list_get(li);

				rc = fn(p->s, arg);
				if (rc == 0)
					return 0;
			}
		}
	}

	return 1;
}

stringmap stringmap_new(size_t nelem)
{
	stringmap sm;

	assert(nelem > 0);

	if ((sm = malloc(sizeof *sm)) == NULL)
		return NULL;

	if ((sm->hashtable = calloc(nelem, sizeof *sm->hashtable)) == NULL) {
		free(sm);
		return NULL;
	}

	sm->nelem = nelem;
	sm->last_id = 0;

	return sm;
}

void stringmap_free(stringmap sm)
{
	size_t i;

	if (sm != NULL) {
		for (i = 0; i < sm->nelem; i++) {
			if (sm->hashtable[i] != NULL)
				list_free(sm->hashtable[i], entry_free);
		}

		free(sm->hashtable);
		free(sm);
	}
}

status_t stringmap_invalidate(stringmap sm)
{
	size_t i;

	assert(sm != NULL);

	for (i = 0; i < sm->nelem; i++) {
		if (sm->hashtable[i] != NULL)
			list_free(sm->hashtable[i], entry_free);
	}

	return success;
}

/* Locate an existing entry and return a list and iterator to it */
static bool sm_locate(stringmap sm, const char *s, list* plist, list_iterator* pi)
{
	unsigned long hashval;
	size_t hid;

	assert(sm != NULL);
	assert(s != NULL);
	assert(strlen(s) > 0);

	hashval = hash((const unsigned char*)s);
	hid = hashval % sm->nelem;
	*plist = sm->hashtable[hid];

	if (*plist != NULL) {
		for (*pi = list_first(*plist); !list_end(*pi); *pi = list_next(*pi)) {
			struct entry* e = list_get(*pi);
			if (e->hashval == hashval && strcmp(s, e->s) == 0)
				return 1;
		}
	}

	return 0;
}

status_t stringmap_add(stringmap sm, const char *s, unsigned long* pid)
{
	struct entry *e = NULL;
	size_t n, hid;
	list lst;
	list_iterator i;

	assert(sm != NULL);
	assert(s != NULL);
	assert(strlen(s) > 0);
	assert(pid != NULL);


	if (sm_locate(sm, s, &lst, &i)) {
		e = list_get(i);

		/* Item already exist. Do not add it. */
		*pid = e->id;
		return success;
	}

	if ((e = malloc(sizeof *e)) == NULL)
		goto err;

	n = strlen(s) + 1;
	if ((e->s = malloc(n)) == NULL)
		goto err;

	memcpy(e->s, s, n);
	e->hashval = hash((const unsigned char*)e->s);
	*pid = e->id = ++sm->last_id;

	/* Add it to the list */
	hid = e->hashval % sm->nelem;

	if (sm->hashtable[hid] == NULL
	&& (sm->hashtable[hid] = list_new()) == NULL)
		goto err;

	if (list_add(sm->hashtable[hid], e) == NULL)
		goto err;

	return success;

err:
	if (e) {
		free(e->s);
		free(e);
	}

	return failure;
}

bool stringmap_exists(stringmap sm, const char *s)
{
	list lst;
	list_iterator i;

	assert(sm != NULL);
	assert(s != NULL);
	assert(strlen(s) > 0);

	return sm_locate(sm, s, &lst, &i);
}

status_t stringmap_get_id(stringmap sm, const char *s, unsigned long* pid)
{
	list lst;
	list_iterator i;
	struct entry* e;

	assert(sm != NULL);
	assert(s != NULL);
	assert(strlen(s) > 0);

	if (!sm_locate(sm, s, &lst, &i))
		return failure;

	e = list_get(i);
	*pid = e->id;
	return success;
}

stringmap stringmap_subset(stringmap sm1, stringmap sm2)
{
	stringmap sm = NULL;
	size_t i;
	unsigned long id;

	if ((sm = stringmap_new(sm1->nelem)) == NULL)
		return NULL;

	/* Check each element in sm1, if it isn't in sm2, then
	 * add it to sm.  */
	for (i = 0; i < sm1->nelem; i++) {
		if (sm1->hashtable[i] != NULL) {
			list_iterator li;
			for (li = list_first(sm1->hashtable[i]); !list_end(li); li = list_next(li)) {
				struct entry* p = list_get(li);
				assert(p != NULL);
				if (!stringmap_exists(sm2, p->s) 
				&& !stringmap_add(sm, p->s, &id))
					goto err;
			}
		}
	}

	return sm;

err:
	stringmap_free(sm);
	return NULL;

}

list stringmap_tolist(stringmap sm)
{
	list lst = NULL;
	size_t i;
	char *s;

	if ((lst = list_new()) == NULL)
		return NULL;

	for (i = 0; i < sm->nelem; i++) {
		if (sm->hashtable[i] != NULL) {
			list_iterator li;

			for (li = list_first(sm->hashtable[i]); !list_end(li); li = list_next(li)) {
				struct entry* p =  list_get(li);
				size_t n = strlen(p->s) + 1;
				if ((s = malloc(n)) == NULL)
					goto err;

				memcpy(s, p->s, n);
				if (!list_add(lst, s))
					goto err;
			}
		}
	}


	return lst;

err:
	list_free(lst, NULL);
	return NULL;
}

#ifdef CHECK_STRINGMAP
int main(void)
{
static const char *data[] = {
	"CVS", "Doxyfile", "Doxyfile.bak", "Makefile", "Makefile.am",
	"Makefile.in", "array.c", "array.h", "array.o", "bitset.c",
	"bitset.h", "bitset.o", "blacksholes.c", "cache.c", "cache.h",
	"cache.o", "configfile.c", "configfile.h", "configfile.o", "connection.c",
	"connection.h", "connection.o", "cstring.c", "cstring.h", "cstring.o",
	"exotic_options.c", "factorial.c", "factorial.o", "filecache.h", "hashmap.c",
	"hashmap.h", "libmeta.a", "membuf.c", "membuf.h", "membuf.o",
	"meta_error.c", "meta_error.h", "meta_error.o", "metadata.c", "metadata.h",
	"metadata.o", "metadate.c", "metadate.h", "metadate.o", "metalist.c",
	"metalist.h", "metalist.o", "metamap.c", "metamap.h", "metamap.o",
	"metamem.c", "metamem.h", "metamem.o", "metaoptions.h", "metatypes.h",
	"miscfunc.c", "miscfunc.h", "miscfunc.o", "normdist.c", "options",
	"pair.c", "pair.h", "pair.o", "pool.c", "pool.h",
	"pool.o", "process.c", "process.h", "process.o", "rfc1738.c",
	"rfc1738.h", "rfc1738.o", "samples", "sock.c", "sock.h",
	"sock.o", "sqlnet.log", "stack.c", "stack.h", "stack.o",
	"stringmap.c", "stringmap.h", "stringmap.o", "table.c", "table.h",
	"table.o", "tcp_server.c", "tcp_server.h", "tcp_server.o", "threadpool.c",
	"threadpool.h", "threadpool.o"
};

	size_t i;
	unsigned long id;
	stringmap sm;

	sm = stringmap_new(10);
	assert(sm != NULL);

	for (i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
		if (!stringmap_add(sm, data[i], &id)) {
			fprintf(stderr, "Error adding item %d\n", (int)i);
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < sizeof(data) / sizeof(data[0]); i++) {
		if (stringmap_exists(sm, data[i])) {
			if (!stringmap_get_id(sm, data[i], &id)) {
				fprintf(stderr, "Could not retrieve id for item %s\n", data[i]);
				exit(EXIT_FAILURE);
			}
		}
		else {
			fprintf(stderr, "Item %s does not exist???\n", data[i]);
			exit(EXIT_FAILURE);
		}
	}

	stringmap_free(sm);
	return 0;
}
#endif
