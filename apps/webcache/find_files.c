#include <stdio.h>
#include <fnmatch.h>
#include <dirent.h>
#include <errno.h>

#include <highlander.h>
#include <meta_filecache.h>
#include <meta_misc.h>
#include <cstring.h>

#include <httpcache.h>

extern filecache g_filecache;

/* Call popen() to get mime type, then niceify it a little and
 * return the mime type in buf.
 */
static void popen_mime_type(const char *filename, char *buf, size_t cb)
{
#if 0
    FILE *f;
    size_t offset;

    snprintf(buf, cb, "file -bi %s", filename);
    if ( (f = popen(buf, "r")) == NULL) {
        perror("popen");
        exit(EXIT_FAILURE);
    }
    if (fgets(buf, cb, f) == NULL) {
        perror("fgets");
        exit(EXIT_FAILURE);
    }
    pclose(f);

    /* Nicify the mime type */
    offset = strcspn(buf, ":;, \t\n");
    buf[offset] = '\0';
#else
    (void)cb;
    const char *s = get_mime_type(filename);
    strcpy(buf, s);
#endif
}


static const char* create_known_as(const char *rootdir, const char *file)
{
    const char *s;

    if (strstr(file, rootdir) != file) {
        fprintf(stderr, "The file %s does not belong to our tree, %s\n", file, rootdir);
        exit(EXIT_FAILURE);
    }

    s = file + strlen(rootdir);
    /* Skip slash, if any */
    if (*s == '/')
        s++;

    return s;
}

/* Returns a list of all new files. Remember to free the list */
list find_new_files(const char *directories, cstring* patterns, size_t npatterns)
{
    list lst = NULL;
    list_iterator i;

    if ( (lst = list_new()) == NULL)
        goto err;

    if (!walk_all_directories(directories, patterns, npatterns, lst, 1))
        goto err;

    /* Now see if all files are int the cache */
    i = list_first(lst);
    while (!list_end(i)) {
        fileinfo fi = list_get(i);
        assert(fi != NULL);
        if (filecache_exists(g_filecache, fileinfo_alias(fi)))
            i = list_delete(lst, i, fileinfo_freev);
        else
            i = list_next(i);
    }

    return lst;

err:
    list_free(lst, fileinfo_freev);
    return NULL;
}

/* Returns a list of all modified files. Remember to free the list */
list find_modified_files(const char *directories, cstring* patterns, size_t npatterns)
{
    list lst = NULL;
    list_iterator i;
    struct stat cachefile;
    const struct stat *diskfile;

    if ( (lst = list_new()) == NULL)
        goto err;

    if (!walk_all_directories(directories, patterns, npatterns, lst, 1))
        goto err;

    /* Now see if all files are int the cache */
    i = list_first(lst);
    while (!list_end(i)) {
        fileinfo fi = list_get(i);
        assert(fi != NULL);

        diskfile = fileinfo_stat(fi);
        assert(diskfile != NULL);

        if (!filecache_stat(g_filecache, fileinfo_alias(fi), &cachefile)
        || cachefile.st_mtime == diskfile->st_mtime)  {
            verbose(3, "File %s is not modified\n", fileinfo_alias(fi));
            i = list_delete(lst, i, fileinfo_freev);
        }
        else  {
            verbose(2, "File %s is modified\n", fileinfo_alias(fi));
            i = list_next(i);
        }
    }

    return lst;

err:
    list_free(lst, fileinfo_freev);
    return NULL;
}

/* To find deleted files, we must start by looking at
 * the contents of the cache. Any file in cache and not
 * on disk, is deleted. Get all files on disk, then walk
 * the cache and see if the file is on disk.
 */
list find_deleted_files(const char *directories, cstring* patterns, size_t npatterns)
{
    list diskfiles = NULL;
    list_iterator li;
    stringmap sm = NULL;
    stringmap deleted = NULL;
    stringmap sm_filecache = filecache_filenames(g_filecache);
    list files = NULL;
    unsigned long id;
    size_t nfiles;

    if ( (diskfiles = list_new()) == NULL)
        goto err;

    if (!walk_all_directories(directories, patterns, npatterns, diskfiles, 0))
        goto err;

    if ((nfiles = list_size(diskfiles)) == 0) {
        /* Found no files at all. Not exactly an internal server error,
         * so we return an empty list.
         */
        list_free(diskfiles, fileinfo_freev);
        return list_new();

    }

    if ((sm = stringmap_new(nfiles)) == NULL)
        goto err;

    /* Convert fileinfo to a stringmap */
    verbose(3, "%s(): Converting fileinfo to a stringmap\n", __func__);
    for (li = list_first(diskfiles); !list_end(li); li = list_next(li)) {
        fileinfo fi = list_get(li);
        const char *s = fileinfo_alias(fi);
        if (!stringmap_add(sm, s, &id))
            goto err;
    }

    /* Now get the deleted nodes */
    verbose(3, "%s(): Finding deleted nodes\n", __func__);
    if ( (deleted = stringmap_subset(sm_filecache, sm)) == NULL)
        goto err;

    /* Convert the stringmap to a list */
    verbose(3, "%s(): Converting deleted nodes to a list\n", __func__);
    if ( (files = stringmap_tolist(deleted)) == NULL)
        goto err;

    verbose(3, "%s(): cleaning up\n", __func__);
    list_free(diskfiles, fileinfo_freev);
    stringmap_free(deleted);
    stringmap_free(sm);
    verbose(3, "%s(): Returning file list with %zu entries\n", __func__, list_size(files));
    return files;

err:
    list_free(diskfiles, fileinfo_freev);
    list_free(files, free);
    stringmap_free(sm);
    stringmap_free(deleted);
    return NULL;
}


status_t walk_all_directories(const char *directories, cstring* patterns, size_t npatterns, list lst, int get_mimetype)
{
    cstring *pstr;
    size_t i, nelem;
    status_t rc = success;

    /* Split the directories argument and then walk each element in the path */
    if ( (nelem = cstring_split(&pstr, directories, " \t")) == 0)
        return failure;

    /* Now walk the tree(s) */
    for (i = 0; i < nelem; i++) {
        const char *rootdir = c_str(pstr[i]);
        if (!find_files(rootdir, rootdir, patterns, npatterns, lst, get_mimetype)) {
            rc = failure;
            break;
        }
    }

    cstring_multifree(pstr, nelem);
    free(pstr);
    return rc;
}

/*
 * Our caller has decided that it has found a file. This function
 * checks to see if that file matches the patterns we want to cache.
 * If so, we create a fileinfo object and add misc info to that object.
 * Error handling:
 * We return 0 if errors, 1 if not.
 */
static int handle_one_file(
    const char *rootdir,
    const char *path,
    cstring* patterns,
    size_t npatterns,
    list lst,
    struct stat *pst,
    int get_mimetype)
{
    int rc = 0;
    size_t i, cb;
    const char *known_as;
    char *base = NULL;

    /* Extract the base name of the object, including
     * extensions. */
    cb = strlen(path) + 1;
    if ( (base = malloc(cb)) == NULL)
        goto err;

    get_basename(path, NULL, base, cb);

    /* See if filename matches patterns */
    for (i = 0; i < npatterns; i++) {
        if (fnmatch(c_str(patterns[i]), base, 0) == 0) {
            fileinfo fi;
            char mimetype[2048];

            if ( (fi = fileinfo_new()) == NULL)
                goto err;

            if (get_mimetype)
                popen_mime_type(path, mimetype, sizeof mimetype);
            else
                mimetype[0] = '\0';

            /* Save info in a node and add it to the list */
            known_as = create_known_as(rootdir, path);
            fileinfo_set_stat(fi, pst);

            if (fileinfo_set_alias(fi, known_as)
            && fileinfo_set_name(fi, path)
            && fileinfo_set_mimetype(fi, mimetype)
            && list_add(lst, fi)) {
                /* Stop checking patterns */
                break;
            }

            /* Crapola, out of memory */
            fileinfo_free(fi);
            goto err;
        }
    }

    /* Set return code to success and fall through to cleanup */
    rc = 1;

err:
    free(base);
    return rc;
}

/*
 * This function finds all files in all dirs matching the patterns
 * from the configuration file. The result is stored in a meta_list
 * for further processing by other functions. We do NOT use ftw()
 * as it isn't thread safe. Wrapping ntw() in a mutex and global list
 * is just too ugly for my taste.
 *
 * Anyway, this function is recursive and will use a lot of file descriptors
 * if the directory tree is deep.
 *
 * Params:
 * rootdir is where we started to traverse the tree. We use it to construct
 * the known_as alias for files
 *
 * curdir is the full path to the current directory(Note: Not getcwd()),
 * we construct this once for each dir we check. We realloc() this variable
 * to make room for the appended strings.
 *
 * dirname is the name of the directory we shall check.
 *
 * patterns&npatterns are the files setting from the configuration file,
 * split up into cstring strings.
 *
 * lst is where we append our data.
 */
status_t find_files(
    const char *rootdir,
    const char *dirname,
    cstring* patterns,
    size_t npatterns,
    list lst,
    int get_mimetype)
{
    DIR *d = NULL;
    status_t rc = failure;

    /* Buffers used for string/path manipulation */
    char *path = NULL;
    size_t cb = 0;

    struct dirent *de = NULL;
    struct stat st;

    /* We return error(0) for anything but permission errors */
    if ( (d = opendir(dirname)) == NULL)
        return errno == EACCES ? success : failure;

    while ((de = readdir(d)) != NULL) {
        /* We do not walk parent or current directory */
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        /* Compute the number of bytes needed for path */
        cb = strlen(dirname) + 1 + strlen(de->d_name) + 1;
        free(path);
        if ( (path = malloc(cb)) == NULL)
            goto err;

        /* Create new path */
        snprintf(path, cb, "%s/%s", dirname, de->d_name);

        /* Get file info */
        if (stat(path, &st) == -1)
            goto err;

        /* Check subdirectory for more files */
        if (S_ISDIR(st.st_mode)) {
            /* We found a directory, check it */
            verbose(2, "Checking path %s...\n", path);
            if (find_files(rootdir, path,  patterns, npatterns, lst, get_mimetype) == 0)
                goto err;
        }
        else if (S_ISREG(st.st_mode)) {
            if (!handle_one_file(rootdir, path, patterns, npatterns, lst, &st, get_mimetype))
                goto err;
        }
    }

    /* Indicate success and fallthrough to cleanup */
    rc = success;

err:
    if (d != NULL)
        closedir(d);

    free(path);
    return rc;
}
