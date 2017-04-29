/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
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
    list lst;
    void(*freefunc)(void *arg);
};

map_iterator map_first(map this)
{
    map_iterator mi;
    list_iterator li;

    li = list_first(this->lst);
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

char *map_key(map_iterator mi)
{
    struct pair *p;
    list_iterator li;

    li.node = mi.node;
    p = list_get(li);
    return p->key;
}

void *map_value(map_iterator mi)
{
    struct pair *p;
    list_iterator li;

    li.node = mi.node;
    p = list_get(li);
    return p->value;
}

map map_new(void(*freefunc)(void *arg))
{
    map new;

    if ((new = calloc(1, sizeof *new)) == NULL)
        return NULL;

    if ((new->lst = list_new()) == NULL) {
        free(new);
        return NULL;
    }

    new->freefunc = freefunc;
    return new;
}

void map_free(map this)
{
    list_iterator i;
    struct pair *entry;

    if (this == NULL)
        return;

    for (i = list_first(this->lst); !list_end(i); i = list_next(i)) {
        entry = list_get(i);
        free(entry->key);
        if (this->freefunc != 0)
            this->freefunc(entry->value);
    }

    list_free(this->lst, free);
    free(this);
}

static struct pair* map_find(map this, const char *key)
{
    list_iterator i;

    for (i = list_first(this->lst); !list_end(i); i = list_next(i)) {
        struct pair *p = list_get(i);

        if (strcmp(key, p->key) == 0)
            return p;
    }

    return NULL;
}

status_t map_set(map this, const char *key, void *value)
{
    struct pair *entry;
    list tmp;
    size_t n;

    /* Update if key already exists. */
    if ((entry = map_find(this, key)) != NULL) {
        if (this->freefunc != 0)
            this->freefunc(entry->value);

        entry->value = value;
        return success;
    }

    if ((entry = malloc(sizeof *entry)) == NULL)
        return failure;

    n = strlen(key) + 1;
    if ((entry->key = malloc(n)) == NULL)  {
        free(entry);
        return failure;
    }

    memcpy(entry->key, key, n);
    entry->value = value;

    tmp = list_add(this->lst, entry);
    if (tmp == NULL) {
        free(entry->key);
        free(entry);
        return failure;
    }
    
    return success;
}

bool map_exists(map this, const char *key)
{
    if (map_find(this, key))
        return true;

    return false;
}

void *map_get(map this, const char *key)
{
    struct pair *pair;

    if ((pair = map_find(this, key)) == NULL)
        return NULL;
        
    return pair->value;
}

int map_foreach(map this, void *args, int(*f)(void *args, char *key, void *data))
{
    list_iterator i;

    for (i = list_first(this->lst); !list_end(i); i = list_next(i)) {
        struct pair *p;

        p = list_get(i);
        if (f(args, p->key, p->value) == 0)
            return 0;
    }

    return 1;
}
