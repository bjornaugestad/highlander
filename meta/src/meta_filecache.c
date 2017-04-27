/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
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

#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>

#include <meta_cache.h>
#include <meta_stringmap.h>
#include <meta_filecache.h>

struct fileinfo_tag {
    struct stat st;
    char *mimetype;
    char *name;
    char *alias;
    void *contents;
};

/* We store a couple of things for each file in a struct like this */
fileinfo fileinfo_new(void)
{
    fileinfo p;

    if ((p = malloc(sizeof *p)) == NULL)
        return NULL;
        
    p->mimetype = NULL;
    p->name = NULL;
    p->alias = NULL;
    p->contents = NULL;

    return p;
}

void fileinfo_free(fileinfo p)
{
    if (p != NULL) {
        free(p->contents);
        free(p->mimetype);
        free(p->alias);
        free(p->name);
        free(p);
    }
}

void fileinfo_set_stat(fileinfo p, const struct stat* pst)
{
    assert(p != NULL);
    assert(pst != NULL);

    p->st = *pst;
}

status_t fileinfo_set_name(fileinfo p, const char *s)
{
    assert(p != NULL);
    assert(s != NULL);

    if (p->name != NULL)
        free(p->name);

    if ((p->name = malloc(strlen(s) + 1)) == NULL)
        return failure;

    strcpy(p->name, s);
    return success;
}

status_t fileinfo_set_alias(fileinfo p, const char *s)
{
    assert(p != NULL);
    assert(s != NULL);

    if (p->alias != NULL)
        free(p->alias);

    if ((p->alias = malloc(strlen(s) + 1)) == NULL)
        return failure;

    strcpy(p->alias, s);
    return success;
}

status_t fileinfo_set_mimetype(fileinfo p, const char *s)
{
    assert(p != NULL);
    assert(s != NULL);

    if (p->mimetype != NULL)
        free(p->mimetype);

    if ((p->mimetype = malloc(strlen(s) + 1)) == NULL)
        return failure;

    strcpy(p->mimetype, s);
    return success;
}


#define HOTLIST_SIZE 10

filecache filecache_new(size_t nelem, size_t bytes)
{
    filecache new;

    assert(nelem > 0);
    assert(bytes > 0);

    if ((new = malloc(sizeof *new)) == NULL
    ||	(new->filenames = stringmap_new(nelem)) == NULL
    ||	(new->metacache = cache_new(nelem, HOTLIST_SIZE, bytes)) == NULL) {
        stringmap_free(new->filenames);
        free(new);
        new = NULL;
    }
    else  {
        pthread_rwlock_init(&new->lock, NULL);
        new->nelem = nelem;
        new->bytes = bytes;
    }

    return new;
}

void filecache_free(filecache this)
{
    if (this != NULL) {
        cache_free(this->metacache, (dtor)fileinfo_free);
        stringmap_free(this->filenames);
        pthread_rwlock_destroy(&this->lock);
        free(this);
    }
}

status_t filecache_add(filecache this, fileinfo finfo, int pin, unsigned long* pid)
{
    int fd = -1;
    status_t rc;

    char *contents = NULL;

    assert(this != NULL);
    assert(finfo != NULL);
    assert(finfo->contents == NULL);

    if ((fd = open(fileinfo_name(finfo), O_RDONLY)) == -1)
        goto err;

    if ((finfo->contents = malloc(finfo->st.st_size)) == NULL)
        goto err;

    if (read(fd, finfo->contents, (size_t)finfo->st.st_size) != (ssize_t)finfo->st.st_size)
        goto err;

    if (close(fd) == -1)
        goto err;

    fd = -1;

    pthread_rwlock_wrlock(&this->lock);
    rc = stringmap_add(this->filenames, fileinfo_alias(finfo), pid);

    if (rc == success)
        rc = cache_add(this->metacache, *pid, finfo, sizeof *finfo, pin);

    pthread_rwlock_unlock(&this->lock);
    if (rc)
        return success;

err:
    if (fd != -1)
        close(fd);

    free(contents);
    fileinfo_free(finfo);
    return failure;
}


int filecache_invalidate(filecache this)
{
    int rc = 1;

    pthread_rwlock_wrlock(&this->lock);
    stringmap_free(this->filenames);
    cache_free(this->metacache, (dtor)fileinfo_free);

    if ((this->filenames = stringmap_new(this->nelem)) == NULL) {
        pthread_rwlock_unlock(&this->lock);
        return 0;
    }

    if ((this->metacache = cache_new(this->nelem, HOTLIST_SIZE, this->bytes)) == NULL) {
        stringmap_free(this->filenames);
        this->filenames = NULL;
        this->metacache = NULL;
    }

    pthread_rwlock_unlock(&this->lock);
    return rc;
}

double filecache_hitratio(filecache this)
{
    (void)this;
    return 1.0;
}

int filecache_exists(filecache this, const char *filename)
{
    unsigned long id;
    int rc = 0;

    pthread_rwlock_rdlock(&this->lock);
    if (stringmap_get_id(this->filenames, filename, &id))
        rc = 1;

    pthread_rwlock_unlock(&this->lock);

    return rc;
}

status_t filecache_get(filecache this, const char *filename, void** pdata, size_t* pcb)
{
    unsigned long id;
    status_t rc = failure;
    void *p;

    pthread_rwlock_rdlock(&this->lock);

    if (stringmap_get_id(this->filenames, filename, &id)) {
        rc = cache_get(this->metacache, id, (void*)&p, pcb);
        if (rc) {
            fileinfo fi = p;
            *pdata = fi->contents;
            *pcb = fi->st.st_size;
        }
    }

    pthread_rwlock_unlock(&this->lock);
    return rc;
}

int filecache_foreach(filecache this, int(*fn)(const char*s, void *arg), void *arg)
{
    int rc;

    assert(this != NULL);
    assert(fn != NULL);

    pthread_rwlock_rdlock(&this->lock);

    rc = stringmap_foreach(this->filenames, fn, arg);
    pthread_rwlock_unlock(&this->lock);
    return rc;

}

status_t filecache_stat(filecache this, const char *filename, struct stat* p)
{
    unsigned long id;
    void *pst = NULL;
    size_t cb;
    status_t rc = failure;

    assert(this != NULL);
    assert(filename != NULL);

    pthread_rwlock_rdlock(&this->lock);

    if (stringmap_get_id(this->filenames, filename, &id)) {
        if (cache_get(this->metacache, id, &pst, &cb)) {
            *p = *fileinfo_stat(pst);
            rc = success;
        }
    }

    pthread_rwlock_unlock(&this->lock);
    return rc;
}

status_t filecache_get_mime_type(filecache this, const char *filename, char mime[], size_t cb)
{
    unsigned long id;
    void *p;
    size_t cbptr;
    status_t rc = failure;

    pthread_rwlock_rdlock(&this->lock);

    if (stringmap_get_id(this->filenames, filename, &id)) {
        if (cache_get(this->metacache, id, (void*)&p, &cbptr)) {
            mime[0] = '\0';
            strncat(mime, fileinfo_mimetype(p), cb - 1);
            rc = success;
        }
    }

    pthread_rwlock_unlock(&this->lock);
    return rc;
}

fileinfo filecache_fileinfo(filecache this, const char *filename)
{
    unsigned long id;
    void *pst = NULL;
    size_t cb;
    status_t rc = failure;

    assert(this != NULL);
    assert(filename != NULL);


    pthread_rwlock_rdlock(&this->lock);

    if (stringmap_get_id(this->filenames, filename, &id))
        rc = cache_get(this->metacache, id, (void*)&pst, &cb);

    pthread_rwlock_unlock(&this->lock);

    if (rc)
        return pst;

    return NULL;
}

const struct stat* fileinfo_stat(fileinfo p)
{
    assert(p != NULL);
    return &p->st;
}

const char *fileinfo_name(fileinfo p)
{
    assert(p != NULL);
    return p->name;
}

const char *fileinfo_alias(fileinfo p)
{
    assert(p != NULL);
    return p->alias;
}

const char *fileinfo_mimetype(fileinfo p)
{
    assert(p != NULL);
    return p->mimetype;
}


#ifdef CHECK_FILECACHE
#include <stdio.h>
#include <dirent.h>

int main(void)
{
    DIR* d;
    struct dirent* de;
    struct stat st;
    filecache fc;
    unsigned long id;
    void *pdata;
    size_t cb;
    FILE* f;
    char mime[128];
    fileinfo fi;


    fc = filecache_new(100, 100*1024*1024);

    if ((d = opendir(".")) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((de = readdir(d)) != NULL) {
        if (stat(de->d_name, &st) == -1) {
            perror("fstat");
            exit(EXIT_FAILURE);
        }

        if (S_ISREG(st.st_mode)) {
            char buf[10240];
            size_t offset;
            sprintf(buf, "file -bi %s", de->d_name);
            if ((f = popen(buf, "r")) == NULL) {
                perror("popen");
                exit(EXIT_FAILURE);
            }
            if (fgets(buf, sizeof buf, f) == NULL) {
                perror("fgets");
                exit(EXIT_FAILURE);
            }
            pclose(f);

            /* Nicify the mime type */
            offset = strcspn(buf, ":;, \t\n");
            buf[offset] = '\0';

            fi = fileinfo_new();
            if (!fileinfo_set_name(fi, de->d_name)) {
                perror(de->d_name);
                exit(EXIT_FAILURE);
            }

            if (!fileinfo_set_alias(fi, de->d_name)) {
                perror(de->d_name);
                exit(EXIT_FAILURE);
            }

            fileinfo_set_stat(fi, &st);

            if (!fileinfo_set_mimetype(fi, buf)) {
                perror(de->d_name);
                exit(EXIT_FAILURE);
            }

            if (st.st_size == 0)
                ; /* Do not add empty files */
            else if (!filecache_add(fc, fi, 0, &id)) {
                perror(de->d_name);
                exit(EXIT_FAILURE);
            }
            else if (!filecache_get(fc, de->d_name, &pdata, &cb)) {
                perror(de->d_name);
                exit(EXIT_FAILURE);
            }
            else if (!filecache_get_mime_type(fc, de->d_name, mime, sizeof mime)){
                perror(de->d_name);
                exit(EXIT_FAILURE);
            }
        }
    }

    closedir(d);
    filecache_free(fc);
    return 0;
}
#endif
