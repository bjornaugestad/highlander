/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include <meta_common.h>
#include <meta_configfile.h>

/*
 * Local helper struct storing name and value for a directive.
 */
struct nameval {
    char *name;
    char *value;
};

#define MAX_DIRECTIVES 2000

/*
 * We keep it simple and just allocate up to MAX_DIRECTIVES pointers
 * to nameval structs. Then we use these pointers and place
 * names and values in them.
 */
struct configfile_tag {
    struct nameval* values[MAX_DIRECTIVES];
    size_t used;
};

/*
 * Returns 0 if no name/value was found, 1 if it was and -1 if syntax
 * error. Note that we write to line.
 */
static int get_name_and_value(char *line, char *name, char *value)
{
    char *s, *org_name = name;
    int quoted = 0;

    /* Remove comments, if any */
    if ((s = strchr(line, '#')) != NULL)
        *s = '\0';

    /* Get first non-ws character */
    s = line;
    while (*s != '\0' && isspace((unsigned char)*s))
        s++;

    /* Was the line ws only? */
    if (*s == '\0')
        return 0;

    /* Now s points to the first char in name.
     * Copy to the param name */
     while (*s != '\0' && !isspace((unsigned char)*s))
         *name++ = *s++;

    /* See if we found a space or not.
     * The space(or tab) is required */
    if (*s == '\0')
        return -1;

    /* Terminate name and look for next non-ws
     * which is the start of the value
     */
    *name = '\0';
    while (*s != '\0' && isspace((unsigned char)*s))
        s++;

    /* Skip leading ", if any */
    if (*s == '"') {
        quoted = 1;
        s++;
    }

    /* Now copy value */
    if (quoted) {
        while (*s != '\0' && *s != '"')
            *value++ = *s++;
    }
    else {
        while (*s != '\0' && !isspace((unsigned char)*s))
            *value++ = *s++;
    }

    /* Terminate value and perform sanity check */
    *value = '\0';
    if (strlen(org_name) == 0)
        return -1;

    return 1;
}

static status_t add(configfile cf, const char *name, const char *value)
{
    char *n = NULL, *v = NULL;

    assert(cf != NULL);
    assert(name != NULL);
    assert(value != NULL);

    if (cf->used >= MAX_DIRECTIVES)
        return failure;

    if ((n = malloc(strlen(name) + 1)) == NULL
    ||	(v = malloc(strlen(value) + 1)) == NULL
    ||	(cf->values[cf->used] = malloc(sizeof(struct nameval))) == NULL) {
        free(n);
        free(v);
        return failure;
    }

    strcpy(n, name);
    strcpy(v, value);
    cf->values[cf->used]->name = n;
    cf->values[cf->used]->value = v;
    cf->used++;
    return success;
}


configfile configfile_read(const char *path)
{
    char line[2048];
    char name[2048];
    char value[2048];

    FILE *f = NULL;
    configfile p = NULL;

    if ((f = fopen(path, "r")) == NULL)
        return NULL;

    if ((p = calloc(1, sizeof *p)) == NULL)
        goto err;

    p->used = 0;
    while (fgets(line, (int)sizeof line, f)) {
        int i = get_name_and_value(line, name, value);
        if (i == -1)
            goto err;

        if (i == 1 && !add(p, name, value))
            goto err;
    }

    fclose(f);
    return p;

err:
    if (f)
        fclose(f);

    if (p)
        configfile_free(p);

    return NULL;
}

static struct nameval* find(configfile cf, const char *name)
{
    size_t i;

    assert(cf != NULL);
    assert(name != NULL);

    for (i = 0; i < cf->used; i++) {
        if (strcmp(name, cf->values[i]->name) == 0)
            return cf->values[i];
    }

    return NULL;
}

bool configfile_exists(configfile cf, const char *name)
{
    assert(cf != NULL);
    assert(name != NULL);

    if (find(cf, name) != NULL)
        return true;

    return false;
}

status_t configfile_get_string(configfile cf, const char *name, char *value, size_t cb)
{
    struct nameval* pnv;

    assert(cf != NULL);
    assert(name != NULL);
    assert(value != NULL);
    assert(cb > 1);

    if ((pnv = find(cf, name)) == NULL)
        return fail(ENOENT);

    if (strlen(pnv->value) + 1 > cb)
        return fail(ENOSPC);

    strcpy(value, pnv->value);
    return success;
}

status_t configfile_get_long(configfile cf, const char *name, long *value)
{
    char sz[20];

    assert(cf != NULL);
    assert(name != NULL);
    assert(value != NULL);

    if (!configfile_get_string(cf, name, sz, sizeof sz))
        return failure;

    errno = 0;
    *value = strtol(sz, NULL, 10);
    if (*value == LONG_MIN || *value == LONG_MAX)
        return failure;

    if (*value == 0 && errno == EINVAL)
        return failure;

    return success;
}

status_t configfile_get_ulong(configfile cf, const char *name, unsigned long *value)
{
    char sz[20];

    assert(cf != NULL);
    assert(name != NULL);
    assert(value != NULL);

    if (!configfile_get_string(cf, name, sz, sizeof sz))
        return failure;

    errno = 0;
    *value = strtol(sz, NULL, 10);
    if (*value == ULONG_MAX)
        return failure;

    if (*value == 0 && errno == EINVAL)
        return failure;

    return success;
}


status_t configfile_get_uint(configfile cf, const char *name, unsigned int *value)
{
    unsigned long tmp;

    assert(cf != NULL);
    assert(name != NULL);
    assert(value != NULL);

    if (!configfile_get_ulong(cf, name, &tmp))
        return failure;

    if (tmp > UINT_MAX)
        return fail(EINVAL);

    *value = (unsigned int)tmp;
    return success;
}

status_t configfile_get_int(configfile cf, const char *name, int *value)
{
    long tmp;

    assert(cf != NULL);
    assert(name != NULL);
    assert(value != NULL);

    if (!configfile_get_long(cf, name, &tmp))
        return failure;

    if (tmp < INT_MIN || tmp > INT_MAX)
        return fail(EINVAL);

    *value = (int)tmp;
    return success;
}

void configfile_free(configfile cf)
{
    if (cf != NULL) {
        size_t i;

        for (i = 0; i < cf->used; i++) {
            free(cf->values[i]->name);
            free(cf->values[i]->value);
            free(cf->values[i]);
        }

        free(cf);
    }
}

#ifdef CHECK_CONFIGFILE
int main(void)
{
    const char *filename = "./configfile.conf";
    char string[1024];
    int nint; long nlong;
    const char *quotedstring = "this is a quoted string";

    configfile cf;


    if ((cf = configfile_read(filename)) == NULL)
        return 77;

    if (!configfile_get_string(cf, "logrotate", string, sizeof string))
        return 77;

    if (!configfile_get_int(cf, "logrotate", &nint))
        return 77;

    if (!configfile_get_long(cf, "logrotate", &nlong))
        return 77;

    if (!configfile_get_string(cf, "quotedstring", string, sizeof string))
        return 77;

    if (strcmp(string, quotedstring)) {
        fprintf(stderr, "Expected %s, got %s\n", quotedstring, string);
        return 77;
    }

    configfile_free(cf);
    return 0;
}
#endif
