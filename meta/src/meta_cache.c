/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <meta_list.h>
#include <meta_cache.h>

/*
 * Data for one cache entry.
 */
struct cache_entry {
    size_t id;
    void *data;
    size_t size;
    size_t used;
    int pinned;	/* Keep in ram, yes/no */
};

/*
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

/*
 * OK, naa har vi en meget rask og fin cache, men ingen
 * oversikt over LRU items. Det maa vi ha.
 * Det er ikke noe poeng i aa vite MRU da lookups er
 * saapass raskt at det ikke er noe tema, men vi maa vite
 * hvilke noder vi skal slette naar cachen er full.
 * Vi skal ikke slette de mest populaere(MRU) ei heller
 * de som er pinned i cachen. Saa hva gjoer vi?
 * - Vi oensker ikke aa traversere hashmapen.
 * - Vi oensker ikke aa telle hvor ofte et item
 *	 er brukt.
 *
 * Vi lager derfor en minikoe, en hotlist, som
 * inneholder n id's. Disse er de sist brukte
 * id's, sortert stigende.(sist brukt foerst).
 * Dette er strengt tatt et ring buffer.
 * Saa sletter vi tilfeldig valgte noder som
 * a) ikke er pinned
 * b) ikke er paa hotlisten.
 * Greit nok?
 */

cache cache_new(size_t nelem, size_t hotlist_nelem, size_t cb)
{
    cache c;

    assert(cb > 0);

    if ((c = malloc(sizeof *c)) == NULL)
        return NULL;

    if ((c->hashtable = calloc(nelem, sizeof *c->hashtable)) == NULL) {
        free(c);
        return NULL;
    }

    if ((c->hotlist = calloc(hotlist_nelem, sizeof *c->hotlist)) == NULL) {
        free(c->hashtable);
        free(c);
        return NULL;
    }

    c->nelem = nelem;
    c->max_bytes = cb;
    c->current_bytes = 0;
    c->hotlist_nelem = hotlist_nelem;

    return c;
}

static void cache_entry_free(struct cache_entry* p)
{
    if (p != NULL) {
        free(p->data);
        free(p);
    }
}

static void cache_entry_freev(void *vp)
{
    cache_entry_free(vp);
}

/* We need to free whatever is in the cache, for which we use the cleanup
 * argument. We also need to free the cache_entry node itself. That node
 * contains a pointer to the data we (optionally) freed with the
 * cleanup function. Hmm... :-(
 *
 * We therefore free like this:
 * if cleanup != NULL, call cleanup for each entry and set p->data to NULL
 * else just call free(p->data) in the cache_entry_free() function.
 */
void cache_free(cache c, dtor cleanup)
{
    size_t i;

    if (cleanup == NULL)
        cleanup = free;

    if (c == NULL)
        return;

    for (i = 0; i < c->nelem; i++) {
        list lst = c->hashtable[i];
        if (lst != NULL) {
            list_iterator li;

            for (li = list_first(lst); !list_end(li); li = list_next(li)) {
                struct cache_entry* entry = list_get(li);
                assert(entry != NULL);
                assert(entry->data != NULL);

                cleanup(entry->data);
                entry->data = NULL;
            }

            list_free(lst, cache_entry_freev);
        }
    }

    free(c->hotlist);
    free(c->hashtable);
    free(c);
}

#if 0
void cache_invalidate(cache c, dtor cleanup)
{
    size_t i;

    assert(c != NULL);

    for (i = 0; i < c->nelem; i++) {
        if (c->hashtable[i] != NULL) {
            list_free(c->hashtable[i], cache_entry_freev);
            c->hashtable[i] = NULL;
        }
    }
}
#endif

static int on_hotlist(cache c, size_t id)
{
    size_t i;

    assert(c != NULL);
    for (i = 0; i < c->hotlist_nelem; i++)
        if (c->hotlist[i] == id)
            return 1;

    return 0;
}

/*
 * Check that we have room for the entry or free items if needed.
 */
static status_t make_space(cache c, size_t cb)
{
    struct cache_entry* entry;
    size_t hid;
    list_iterator i;

    assert(c != NULL);

    /* Can the item fit at all? */
    if (cb > c->max_bytes)
        return fail(ENOSPC);

    for (;;) {
        /* Do we have space? */
        if (c->current_bytes + cb <= c->max_bytes)
            return success;

        /* Locate an item to remove from the cache.
         * We just pick one of the lists by random
         * and find an item to delete.
         */
        hid = (size_t)rand() % c->nelem;

        /* Does the slot have a list? If not, pick another */
        if (c->hashtable[hid] == NULL)
            continue;

        /* Iterate through the list and delete an item */
        for (i =list_first(c->hashtable[hid]); !list_end(i); i = list_next(i)) {
            entry = list_get(i);

            if (!on_hotlist(c, entry->id) && !entry->pinned) {
                if (!cache_remove(c, entry->id))
                    return failure;
            }
        }
    }

    /* We failed to free enough objects */
    return fail(ENOSPC);
}

status_t cache_add(cache c, size_t id, void *data, size_t cb, int pin)
{
    struct cache_entry *entry;
    size_t hid = id % c->nelem;

    assert(c != NULL);
    assert(data != NULL);

    if (!make_space(c, cb))
        return failure;

    if ((entry = malloc(sizeof *entry)) == NULL)
        return failure;

    entry->id = id;
    entry->data = data;
    entry->size = cb;
    entry->pinned = pin;

    if (cache_exists(c, id)) {
        /* Hmm, duplicate. We don't like that (for now) */
        free(entry);
        abort();
    }

    if (c->hashtable[hid] == NULL
    && (c->hashtable[hid] = list_new()) == NULL) {
        free(entry);
        return failure;
    }

    if (list_add(c->hashtable[hid], entry) == NULL) {
        free(entry);
        return failure;
    }

    return success;
}

/*
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
    if (c->hotlist[0] == id)
        return;

    for (i = 0; i < c->hotlist_nelem; i++) {
        if (c->hotlist[i] == id) {
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
    struct cache_entry *entry;
    size_t hid;
    list_iterator i;

    assert(c != NULL);

    i.node = NULL; /* Which means list_end() */
    hid = id % c->nelem;
    if (c->hashtable[hid] == NULL)
        return i;

    for (i = list_first(c->hashtable[hid]); !list_end(i); i = list_next(i)) {
        entry = list_get(i);
        assert(entry != NULL);

        if (entry->id == id)
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

status_t cache_get(cache c, size_t id, void** pdata, size_t* pcb)
{
    struct cache_entry *entry;
    list_iterator i;

    assert(c != NULL);
    assert(pdata != NULL);
    assert(pcb != NULL);

    i = find_entry(c, id);
    if (list_end(i))
        return failure;

    entry = list_get(i);
    *pdata = entry->data;
    *pcb = entry->size;
    add_to_hotlist(c, entry->id);

    return success;
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

    for (i = 0; i < c->hotlist_nelem; i++) {
        if (c->hotlist[i] == id) {
            if (i + 1 != c->hotlist_nelem)
                memmove(&c->hotlist[i], &c->hotlist[i + 1], (c->hotlist_nelem - i - 1) * sizeof *c->hotlist);

            c->hotlist[c->hotlist_nelem - 1] = 0;
            return;
        }
    }
}

status_t cache_remove(cache c, size_t id)
{
    list_iterator i;

    assert(c != NULL);

    i = find_entry(c, id);
    if (list_end(i))
        return fail(ENOENT);

    assert(c->hashtable[id % c->nelem] != NULL);
    assert(cache_exists(c, id));

    list_delete(c->hashtable[id % c->nelem], i, cache_entry_freev);
    remove_from_hotlist(c, id);
    return success;
}

#ifdef CHECK_CACHE
#include <time.h>

int main(void)
{
    size_t i, nelem = 10 * 1;
    char *data;
    cache c;
    clock_t start, stop;
    double diff;
    status_t rc;
    char buf[1024];

    c = cache_new(nelem / 10, 10, 1024 * 1024 * 40);

    start = clock();
    for (i = 0; i < nelem; i++) {
        data = malloc(50);
        sprintf(data, "streng %lu", (unsigned long)i);


        if (!cache_exists(c, i)) {
            rc = cache_add(c, i, data, 50, 0);
            assert(rc && "Could not add items to cache");
        }
    }

#if 1

    stop = clock();
    diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
    if (0) fprintf(stderr, "inserts: %lu items in %f seconds\n", (unsigned long)nelem, diff);

    start = clock();
    for (i = 0; i < nelem; i++) {
        void *xdata;
        size_t cb;

        rc = cache_get(c, i, &xdata, &cb);
        assert(rc && "Could not find item");

        sprintf(buf, "streng %lu", (unsigned long)i);
        assert(memcmp(buf, xdata, strlen(xdata)) == 0);
    }

    stop = clock();
    diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
    if (0) fprintf(stderr, "seq. read: %lu items in %f seconds\n", (unsigned long)nelem, diff);

    /* Random retrieval */
    start = clock();
    srand(time(NULL));
    for (i = 0; i < nelem; i++) {
        void *xdata;
        size_t cb;

        rc = cache_get(c, rand() % nelem, &xdata, &cb);
        assert(rc && "rand:Could not find item");
    }

    stop = clock();
    diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
    if (0) fprintf(stderr, "random read: %lu items in %f seconds\n", (unsigned long)nelem, diff);

    /* Remove all items, one by one */
    start = clock();
    for (i = 0; i < nelem; i++) {
        rc = cache_remove(c, i);
        assert(rc && "rand:Could not remove item");
    }

    stop = clock();
    diff = (stop - start) * 1.0 / CLOCKS_PER_SEC;
    if (0) fprintf(stderr, "Removed items: %lu items in %f seconds\n", (unsigned long)nelem, diff);

#endif

    cache_free(c, free);
    (void)rc; // make gcc shut up
    return 0;
}

#endif
