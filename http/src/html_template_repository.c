/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>

#include <meta_html.h>

#include <meta_map.h>

/*
 * A template repository is a thread safe storage for html_templates.
 * An application can contain multiple templates and each thread need
 * some thread safe way to get hold of templates after they've been created.
 * We don't want global variables, but implement a singleton repository
 * instead.
 */
struct html_template_repository {
    map templates;
};

static struct html_template_repository* repos = NULL;


void html_template_repository_empty(void)
{
    if (repos != NULL) {
        map_free(repos->templates);
        free(repos);
        repos = NULL;
    }
}

static int init_repos(void)
{
    if (repos == NULL) {
        if ((repos = malloc(sizeof *repos)) == NULL)
            return 0;

        if ((repos->templates = map_new((void(*)(void*))html_template_free)) == NULL) {
            free(repos);
            repos = NULL;
            return 0;
        }
    }

    return 1;
}

html_buffer html_template_repository_use(const char* template)
{
    html_template t;
    html_buffer b;

    if (!init_repos())
        return NULL;

    if ((t = map_get(repos->templates, template)) == NULL)
        return NULL;

    if ((b = html_buffer_new()) == NULL)
        return NULL;

    html_buffer_set_template(b, t);
    return b;
}

int html_template_repository_add(const char* name, html_template t)
{
    if (!init_repos())
        return 0;

    if (!map_set(repos->templates, name, t))
        return 0;

    return 1;
}
