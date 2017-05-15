/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
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

int stringmap_foreach(stringmap this, int(*fn)(const char*s, void *arg), void *arg)
{
    int rc;
    size_t i;

    assert(this != NULL);
    assert(arg != NULL);

    for (i = 0; i < this->nelem; i++) {
        if (this->hashtable[i] != NULL) {
            list lst = this->hashtable[i];
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
    stringmap new;

    assert(nelem > 0);

    if ((new = malloc(sizeof *new)) == NULL)
        return NULL;

    if ((new->hashtable = calloc(nelem, sizeof *new->hashtable)) == NULL) {
        free(new);
        return NULL;
    }

    new->nelem = nelem;
    new->last_id = 0;

    return new;
}

void stringmap_free(stringmap this)
{
    size_t i;

    if (this != NULL) {
        for (i = 0; i < this->nelem; i++) {
            if (this->hashtable[i] != NULL)
                list_free(this->hashtable[i], entry_free);
        }

        free(this->hashtable);
        free(this);
    }
}

status_t stringmap_invalidate(stringmap this)
{
    size_t i;

    assert(this != NULL);

    for (i = 0; i < this->nelem; i++) {
        if (this->hashtable[i] != NULL)
            list_free(this->hashtable[i], entry_free);
    }

    return success;
}

/* Locate an existing entry and return a list and iterator to it */
static bool sm_locate(stringmap this, const char *s, list* plist, list_iterator* pi)
{
    unsigned long hashval;
    size_t hid;

    assert(this != NULL);
    assert(s != NULL);
    assert(strlen(s) > 0);

    hashval = hash((const unsigned char*)s);
    hid = hashval % this->nelem;
    *plist = this->hashtable[hid];

    if (*plist != NULL) {
        for (*pi = list_first(*plist); !list_end(*pi); *pi = list_next(*pi)) {
            struct entry* e = list_get(*pi);
            if (e->hashval == hashval && strcmp(s, e->s) == 0)
                return 1;
        }
    }

    return 0;
}

status_t stringmap_add(stringmap this, const char *s, unsigned long* pid)
{
    struct entry *e = NULL;
    size_t n, hid;
    list lst;
    list_iterator i;

    assert(this != NULL);
    assert(s != NULL);
    assert(strlen(s) > 0);
    assert(pid != NULL);


    if (sm_locate(this, s, &lst, &i)) {
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
    *pid = e->id = ++this->last_id;

    /* Add it to the list */
    hid = e->hashval % this->nelem;

    if (this->hashtable[hid] == NULL
    && (this->hashtable[hid] = list_new()) == NULL)
        goto err;

    if (list_add(this->hashtable[hid], e) == NULL)
        goto err;

    return success;

err:
    if (e) {
        free(e->s);
        free(e);
    }

    return failure;
}

bool stringmap_exists(stringmap this, const char *s)
{
    list lst;
    list_iterator i;

    assert(this != NULL);
    assert(s != NULL);
    assert(strlen(s) > 0);

    return sm_locate(this, s, &lst, &i);
}

status_t stringmap_get_id(stringmap this, const char *s, unsigned long* pid)
{
    list lst;
    list_iterator i;
    struct entry* e;

    assert(this != NULL);
    assert(s != NULL);
    assert(strlen(s) > 0);

    if (!sm_locate(this, s, &lst, &i))
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

list stringmap_tolist(stringmap this)
{
    list lst = NULL;
    size_t i;
    char *s = NULL;

    if ((lst = list_new()) == NULL)
        return NULL;

    for (i = 0; i < this->nelem; i++) {
        if (this->hashtable[i] != NULL) {
            list_iterator li;

            for (li = list_first(this->hashtable[i]); !list_end(li); li = list_next(li)) {
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
    list_free(lst, free);
    free(s);
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

    size_t i, n;
    unsigned long id;
    stringmap sm;

    sm = stringmap_new(10);
    assert(sm != NULL);

    n = sizeof data / sizeof *data;
    for (i = 0; i < n; i++) {
        if (!stringmap_add(sm, data[i], &id)) {
            fprintf(stderr, "Error adding item %d\n", (int)i);
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < n; i++) {
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
