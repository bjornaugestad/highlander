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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <meta_list.h>
#include <meta_cache.h>

/**
 * Data for one cache entry.
 */
struct cache_entry {
	size_t id;
	void* data;
	size_t size;
	size_t used;
	int pinned;	/* Keep in ram, yes/no */
};

/**
 * Implementation of our cache ADT.
 */
struct cache_tag {
	size_t max_bytes;
	size_t current_bytes;

	size_t nelem;
	list* hashtable;

	size_t* hotlist;/* MRU items */
	size_t hotlist_nelem;
};

/* OK, nå har vi en meget rask og fin cache, men ingen 
 * oversikt over LRU items. Det må vi ha. 
 * Det er ikke noe poeng i å vite MRU da lookups er 
 * såpass raskt at det ikke er noe tema, men vi må vite 
 * hvilke noder vi skal slette når cachen er full. 
 * Vi skal ikke slette de mest populære(MRU) ei heller
 * de som er pinned i cachen. Så hva gjør vi?
 * - Vi ønsker ikke å traversere hashmapen. 
 * - Vi ønsker ikke å telle hvor ofte et item
 *   er brukt.
 *
 * Vi lager derfor en minikø, en hotlist, som
 * inneholder n id's. Disse er de sist brukte
 * id's, sortert stigende.(sist brukt først).
 * Dette er strengt tatt et ring buffer.
 * Så sletter vi tilfeldig valgte noder som
 * a) ikke er pinned
 * b) ikke er på hotlisten.
 * Greit nok?
 */

cache cache_new(size_t nelem, size_t hotlist_nelem, size_t cb)
{
	cache c;

	assert(cb > 0);

	if( (c = mem_malloc(sizeof *c)) != NULL
	&& (c->hashtable = mem_calloc(nelem, sizeof *c->hashtable)) != NULL
	&& (c->hotlist = mem_calloc(hotlist_nelem, sizeof *c->hotlist)) != NULL) {
	
		c->nelem = nelem;
		c->max_bytes = cb;
		c->current_bytes = 0;
		c->hotlist_nelem = hotlist_nelem;
	}
	else {
		if(c != NULL) 
			mem_free(c->hashtable);

		mem_free(c);
		c = NULL;
	}

	return c;
}

static void cache_entry_free(struct cache_entry* p)
{
	if(p != NULL) {
		mem_free(p->data);
		mem_free(p);
	}
}

/* We need to free whatever is in the cache, for which we use the cleanup argument.
 * We also need to free the cache_entry node itself. That node contains a pointer
 * to the data we (optionally) freed with the cleanup function. Hmm... :-(
 *
 * We therefore free like this:
 * if cleanup != NULL, call cleanup for each entry and set p->data to NULL
 * else just call mem_free(p->data) in the cache_entry_free() function.
 */
void cache_free(cache c, dtor cleanup)
{
	if(cleanup == NULL)
		cleanup = free;

	if(c != NULL) {
		size_t i;

		for(i = 0; i < c->nelem; i++) {
			list lst = c->hashtable[i];
			if(lst != NULL) {
				if(cleanup != NULL) {
					list_iterator li;
					for(li = list_first(lst); !list_end(li); li = list_next(li)) {
						struct cache_entry* p = list_get(li);
						assert(p != NULL);
						assert(p->data != NULL);
						cleanup(p->data);
						p->data = NULL;
					}
				}
				list_free(lst, (dtor)cache_entry_free);
			}
		}

		mem_free(c->hotlist);
		mem_free(c->hashtable);
		mem_free(c);
	}
}

#if 0
void cache_invalidate(cache c, dtor cleanup)
{
	size_t i;

	assert(c != NULL);

	for(i = 0; i < c->nelem; i++) {
		if(c->hashtable[i] != NULL) {
			list_free(c->hashtable[i], (dtor)cache_entry_free);
			c->hashtable[i] = NULL;
		}
	}
}
#endif
static int on_hotlist(cache c, size_t id)
{
	size_t i;

	assert(c != NULL);
	for(i = 0; i < c->hotlist_nelem; i++) 
		if(c->hotlist[i] == id)
			return 1;

	return 0;
}

/*
 * Check that we have room for the entry or free items if needed. 
 */
static int make_space(cache c, size_t cb)
{
	struct cache_entry* p;
	size_t hid;
	list_iterator i;

	assert(c != NULL);

	/* Can the item fit at all? */
	if(cb > c->max_bytes) {
		errno = ENOSPC;
		return 0;
	}

	for(;;) {
		/* Do we have space? */
		if(c->current_bytes + cb <= c->max_bytes) 
			return 1;

		/* Locate an item to remove from the cache.
		 * We just pick one of the lists by random 
		 * and find an item to delete. 
		 */
		hid = (size_t)rand() % c->nelem;

		/* Does the slot have a list? If not, pick another */
		if(c->hashtable[hid] == NULL)
			continue;

		/* Iterate through the list and delete an item */
		for(i =list_first(c->hashtable[hid]); !list_end(i); i = list_next(i)) {
			p = list_get(i);
			if(!on_hotlist(c, p->id) && !p->pinned)
				cache_remove(c, p->id);
		}
	}

	/* We failed to free enough objects */
	errno = ENOSPC;
	return 0;
}

	
int cache_add(cache c, size_t id, void* data, size_t cb, int pin)
{
	struct cache_entry *p;
	size_t hid = id % c->nelem;
	int rc = 0;

	assert(c != NULL);
	assert(data != NULL);

	if(!make_space(c, cb))
		;
	else if( (p = mem_malloc(sizeof *p)) == NULL)
		;
	else {
		p->id = id;
		p->data = data;
		p->size = cb;
		p->pinned = pin;

		if(cache_exists(c, id)) {
			/* Hmm, duplicate. We don't like that (for now) */
			mem_free(p);
			assert(0);
		}
		else if(c->hashtable[hid] == NULL
		&& (c->hashtable[hid] = list_new()) == NULL) {
			mem_free(p);
		}
		else if(list_add(c->hashtable[hid], p) == NULL) {
			mem_free(p);
		}
		else 
			rc = 1;
	}

	return rc;
}

/** 
 * Add an item to the hotlist by moving old items to the back
 * and adding the new id in front of the hotlist.
 * Special case: The item may already be in the list, but not
 * in front.
 */
static void add_to_hotlist(cache c, size_t id)
{
	size_t i;

	assert(c != NULL);

	/* Already at front */
	if(c->hotlist[0] == id)
		return;

	for(i = 0; i < c->hotlist_nelem; i++) {
		if(c->hotlist[i] == id) {
			/* id already in hot list, move it to front.
			 * We do that by moving nelem 0..i-1 to 1..i
			 * and then use elem 0 to store id. We know that
			 * i > 0 because of the first test in this function,
			 * so (i - 1) is OK.
			 */
			memmove(&c->hotlist[1], &c->hotlist[0], (i - 1) * sizeof *c->hotlist);
			c->hotlist[0] = id;
			return;
		}
	}

	/* The id was not in the hotlist. Place it at the beginning */
	memmove(&c->hotlist[1], &c->hotlist[0], (c->hotlist_nelem - 1) * sizeof *c->hotlist);
	c->hotlist[0] = id;
}

static inline list_iterator find_entry(cache c, size_t id)
{
	struct cache_entry *p;
	size_t hid;
	list_iterator i;

	assert(c != NULL);

	i.node = NULL; /* Which means list_end() */
	hid = id % c->nelem;
	if(c->hashtable[hid] == NULL)
		return i;

	for(i = list_first(c->hashtable[hid]); !list_end(i); i = list_next(i)) {
		p = list_get(i);
		assert(p != NULL);

		if(p->id == id) 
			break;
	}

	return i;
}

int cache_exists(cache c, size_t id)
{
	list_iterator i;

	assert(c != NULL);

	i = find_entry(c, id);
	return !list_end(i);
}

int cache_get(cache c, size_t id, void** pdata, size_t* pcb)
{
	struct cache_entry *p;
	list_iterator i;
	
	assert(c != NULL);
	assert(pdata != NULL);
	assert(pcb != NULL);

	i = find_entry(c, id);
	if(!list_end(i)) {
		p = list_get(i);
		*pdata = p->data;
		*pcb = p->size;
		add_to_hotlist(c, p->id);
		return 1;
	}

	return 0;
}

/*
 * Removes an item from the hotlist. 
 * Special case: What if the item to be removed is the last one?
 * Then we just zero it.
 */
static void remove_from_hotlist(cache c, size_t id)
{
	size_t i;

	assert(c != NULL);

	for(i = 0; i < c->hotlist_nelem; i++) {
		if(c->hotlist[i] == id) {
			if(i + 1 != c->hotlist_nelem)
				memmove(&c->hotlist[i], &c->hotlist[i + 1], (c->hotlist_nelem - i - 1) * sizeof *c->hotlist);

			c->hotlist[c->hotlist_nelem - 1] = 0;
			return;
		}
	}
}

int cache_remove(cache c, size_t id)
{
	list_iterator i;

	assert(c != NULL);

	i = find_entry(c, id);
	if(list_end(i)) {
		errno = ENOENT;
		return 0;
	}
	else {
		assert(c->hashtable[id % c->nelem] != NULL);
		assert(cache_exists(c, id));

		list_delete(c->hashtable[id % c->nelem], i, (dtor)cache_entry_free);
		remove_from_hotlist(c, id);
		return 1;
	}
}

#ifdef CHECK_CACHE
#include <time.h>

int main(void) 
{
	size_t i, nelem = 10 * 1;
	char* data;
	cache c;
	clock_t start, stop;
	double diff;
	int rc;
	char buf[1024];

	c = cache_new(nelem / 10, 10, 1024 * 1024 * 40);

	start = clock();
	for(i = 0; i < nelem; i++) {
		data = mem_malloc(50);
		sprintf(data, "streng %lu", (unsigned long)i);


		if(!cache_exists(c, i)) {
			rc = cache_add(c, i, data, 50, 0);
			assert(rc && "Could not add items to cache");
		}
	}

#if 1

	stop = clock();
	diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
	fprintf(stderr, "inserts: %lu items in %f seconds\n", (unsigned long)nelem, diff);

	start = clock();
	for(i = 0; i < nelem; i++) {
		void* xdata;
		size_t cb;

		rc = cache_get(c, i, (void**)&xdata, &cb);
		assert(rc && "Could not find item");

		sprintf(buf, "streng %lu", (unsigned long)i);
		assert(memcmp(buf, xdata, strlen(xdata)) == 0);
	}

	stop = clock();
	diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
	fprintf(stderr, "seq. read: %lu items in %f seconds\n", (unsigned long)nelem, diff);

	/* Random retrieval */
	start = clock();
	srand(time(NULL));
	for(i = 0; i < nelem; i++) {
		void* xdata;
		size_t cb;

		rc = cache_get(c, rand() % nelem, (void**)&xdata, &cb);
		assert(rc && "rand:Could not find item");
	}

	stop = clock();
	diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
	fprintf(stderr, "random read: %lu items in %f seconds\n", (unsigned long)nelem, diff);

	/* Remove all items, one by one */
	start = clock();
	for(i = 0; i < nelem; i++) {
		rc = cache_remove(c, i);
		assert(rc && "rand:Could not remove item");
	}

	stop = clock();
	diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
	fprintf(stderr, "Removed items: %lu items in %f seconds\n", (unsigned long)nelem, diff);

#endif

	cache_free(c, free);
	return 0;
}

#endif

