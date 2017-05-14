/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <cstring.h>
#include <meta_list.h>
#include <meta_html.h>

struct html_menu_tag {
    cstring text;
    cstring image;
    cstring hover_image;
    cstring link;
    list items;
};


html_menu html_menu_new(void)
{
    html_menu m;
    cstring arr[4];

    if ((m = malloc(sizeof *m)) == NULL)
        return NULL;

    if ((m->items = list_new()) == NULL) {
        free(m);
        return NULL;
    }

    if (!cstring_multinew(arr, 4)) {
        list_free(m->items, free);
        free(m);
        return NULL;
    }

    m->text = arr[0];
    m->image = arr[1];
    m->hover_image = arr[2];
    m->link = arr[3];

    return m;
}

void html_menu_free(html_menu m)
{
    if (m != NULL) {
        cstring_free(m->text);
        cstring_free(m->image);
        cstring_free(m->hover_image);
        cstring_free(m->link);
        list_free(m->items, (void(*)(void*))html_menu_free);
        free(m);
    }
}

status_t html_menu_set_text(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_set(m->text, s);
}

status_t html_menu_set_image(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_set(m->image, s);
}

status_t html_menu_set_hover_image(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_set(m->hover_image, s);
}

status_t html_menu_set_link(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_set(m->link, s);
}

status_t html_menu_add_menu(html_menu m, html_menu submenu)
{
    assert(m != NULL);
    assert(submenu != NULL);

    return list_add(m->items, submenu) != NULL ? success : failure;
}

const char* html_menu_get_text(html_menu m)
{
    assert(m != NULL);
    return c_str(m->text);
}

const char* html_menu_get_image(html_menu m)
{
    assert(m != NULL);
    return c_str(m->image);
}

const char* html_menu_get_hover_image(html_menu m)
{
    assert(m != NULL);
    return c_str(m->hover_image);
}

const char* html_menu_get_link(html_menu m)
{
    assert(m != NULL);
    return c_str(m->link);
}

/*
 * Profiling shows us that we must pre-render the menu and store
 * the results internally. The menu struct contains a cstring member,
 * rendered, which initially is NULL. Render the data to that buffer
 * and return a pointer to it, or NULL if an error occurs.
 */
int html_menu_render(html_menu m, cstring buffer)
{
    const char *text, *link, *image, *hover_image;
    html_menu submenu;
    list_iterator i;

    text = html_menu_get_text(m);
    link = html_menu_get_link(m);
    image = html_menu_get_image(m);
    hover_image = html_menu_get_hover_image(m);
    (void)image;
    (void)hover_image;

    if (text != NULL) {
        if (link != NULL) {
            if (!cstring_printf(buffer, "<a href='%s'>%s</a><br>\n", link, text))
                return 0;
        }
        else if (!cstring_concat(buffer, text))
            return 0;
    }

    if (m->items != NULL) {
        for (i = list_first(m->items); !list_end(i); i = list_next(i)) {
            submenu = list_get(i);
            if (!html_menu_render(submenu, buffer))
                return 0;
        }
    }

    return 1;
}
