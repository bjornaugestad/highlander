/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn.augestad@gmail.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
        ;
    else if((m->items = list_new()) == NULL
    || !cstring_multinew(arr, 4)) {
        list_free(m->items, NULL);
        free(m);
        m = NULL;
    }
    else {
        m->text = arr[0];
        m->image = arr[1];
        m->hover_image = arr[2];
        m->link = arr[3];
    }

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

int html_menu_set_text(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_copy(m->text, s);
}

int html_menu_set_image(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_copy(m->image, s);
}

int html_menu_set_hover_image(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_copy(m->hover_image, s);
}

int html_menu_set_link(html_menu m, const char* s)
{
    assert(m != NULL);
    assert(s != NULL);
    return cstring_copy(m->link, s);
}

int html_menu_add_menu(html_menu m, html_menu submenu)
{
    assert(m != NULL);
    assert(submenu != NULL);

    return list_add(m->items, submenu) != NULL ? 1 : 0;
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
            size_t cb = strlen(link) + strlen(text) + 100;
            if (!cstring_printf(buffer, cb, "<a href='%s'>%s</a><br>\n", link, text))
                return 0;
        }
        else if(!cstring_concat(buffer, text))
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
