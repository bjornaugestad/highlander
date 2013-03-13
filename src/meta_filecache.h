#ifndef META_FILECACHE_H
#define META_FILECACHE_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <meta_stringmap.h>
#include <meta_cache.h>

/*
 * TODO:
 * - Add function that reports how much of the cache that currently
 * is used.
 */

/**
 * @file meta_filecache.h 
 * @brief Cache files in memory.
 * 
 * This ADT reads files from disk and caches them in memory.
 * The files can later be accessed directly in user space without
 * any kernel calls. This is A Good Thing as it avoids 
 * slow context switches.
 *
 * You can specify both the number of files you want to be
 * able to cache, as well as the total number of bytes
 * you want the cache to use.
 *
 * The file name is used to identify each file, and the file cache
 * ignores symbolic links.
 *
 * The file cache is fully thread safe, and the initial design
 * was done to support the Highlander when running as an
 * image server.
 *
 * Algorithm: The file cache implements a traditional LRU
 * algorithm and will remove the LRU entry when the cache is
 * full and a new request is made. See cache.h for more details
 * as filecache just builds on cache.
 */

/** Declaration of our filecache ADT */
typedef struct filecache_tag* filecache;

/* What we store about each file. */
typedef struct fileinfo_tag* fileinfo;
struct fileinfo_tag {
	struct stat st;
	char *mimetype;
	char *name;
	char *alias;
	void *contents;
};

/**
 * Implementation of the filecache ADT.
 * The file name is used as 'primary key' and stored in the filenames
 * stringmap.  the id from the stringmap will be used as
 * key when we add other properties in the other caches.
 */
struct filecache_tag {
	stringmap filenames;
	cache metacache; /* For struct stat entries */
	pthread_rwlock_t lock;
	size_t nelem;
	size_t bytes;
};

fileinfo fileinfo_new(void);
void     fileinfo_free(fileinfo p);

static inline const struct stat* fileinfo_stat(fileinfo p)
{
	assert(p != NULL);
	return &p->st;
}

static inline const char* fileinfo_name(fileinfo p)
{
	assert(p != NULL);
	return p->name;
}

static inline const char* fileinfo_alias(fileinfo p)
{
	assert(p != NULL);
	return p->alias;
}

static inline const char* fileinfo_mimetype(fileinfo p)
{
	assert(p != NULL);
	return p->mimetype;
}

int fileinfo_set_stat(fileinfo p, const struct stat* pst);
int fileinfo_set_name(fileinfo p, const char* s);
int fileinfo_set_alias(fileinfo p, const char* s);
int fileinfo_set_mimetype(fileinfo p, const char* s);


/**
 * Creates a new filecache. 
 * nelem is used to presize a table of entries in the cache,
 * so that we can perform lookups faster than if using a
 * dynamic solution. 
 *
 * bytes are the total number of bytes the cache can use.
 * A megabyte is 1024*1024, btw.
 * nelem is the number of elements in the hash table(s),
 * and not the max number of elements in the cache.
 */
filecache filecache_new(size_t nelem, size_t bytes);

/**
 * Frees a filecache previously created with filecache_new()
 */
void filecache_free(filecache fc);

/**
 * Reads a file into the file cache. Use it to add entries 
 * to the cache e.g. before you start a service.
 *
 * We read the file named by filename and refer to it by the
 * name it is known_as. This is done to be able to handle
 * relative paths.
 *
 * NOTE: The filecache will own the fileinfo pointer after a
 * call to filecache_add. DO NOT FREE IT EVEN IF filecache_add()
 * RETURNS 0!
 *
 * NOTE: The stat member MUST be valid and up to date. Filecache_add
 * will use that member to allocate memory and read file contents.
 * In most cases we already have stat()'ed the file so this is
 * a non-issue for most. We save a call to stat() by requiring this.
 *
 */ 
int filecache_add(filecache fc, fileinfo finfo, int pin, unsigned long* pid);


/* Return the stringmap member.
 * Note: Treat this one as readonly unless you want weird bugs.
 */
static inline stringmap filecache_filenames(filecache fc)
{
	return fc->filenames;
}


/**
 * Invalidates the whole cache. All memory used by cached
 * objects are freed and all objects are removed. The cache
 * itself is still usable, so you can start adding entries
 * immediately after a call to this function.
 */
int filecache_invalidate(filecache fc);

/**
 * Returns the percentage of files read from disk versus 
 * files read from cache.
 * Not implemented...
 * @see filecache_get
 */
double filecache_hitratio(filecache fc);

/**
 * Gets a file from the cache. 
 */
int filecache_get(filecache fc, const char* filename, void** pdata, size_t* pcb);
int filecache_get_mime_type(filecache fc, const char* filename, char mime[], size_t cb);


/* Returns 1 if a file exists, 0 if not */
int filecache_exists(filecache fc, const char* filename);

/**
 * Return a pointer to a struct stat object from the time we added
 * the file to the filecache, or NULL if the file isn't found.
 */
int filecache_stat(filecache fc, const char* filename, struct stat* p);


/* Call fn once for each element in the filecache, providing name of file
 * as argument s. 
 * Function locks the filecache, so beware of deadlocks.
 */
int filecache_foreach(filecache fc, int(*fn)(const char*s, void* arg), void* arg);

/*
 * Return a pointer to the fileinfo metadata struct for a file, or NULL
 * if the file doesn't exist. 
 */
fileinfo filecache_fileinfo(filecache fc, const char* alias);


#endif

