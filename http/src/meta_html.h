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

#ifndef META_HTML_H
#define META_HTML_H

/* These files will probably be moved to highlander.h
 * later on. First we must implement and test them properly.
 */

#include <cstring.h>
#include <highlander.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Constants */

/* Datatypes */
typedef struct html_section_tag* html_section;
typedef struct html_menu_tag* html_menu;
typedef struct html_buffer_tag* html_buffer;

/*
 * A html template describes a html page. The template can contain
 * multiple sections which will be added to the page in sequence.
 * Two sections have special meaning, those are the menu section and
 * the user section. The menu section will contain the output from
 * the html_menu_render() function, which will be called when it is
 * the menu sections turn to be added to the html page.
 *
 * The user section will contain data printed to a html buffer by
 * a page handler function. The page handler function is called when
 * a request is made for a specific URL. The function can choose to
 * use a template to output its data, but doesn't have to.
 *
 * There's nothing magic with a HTML template, you just add sections
 * and associated code to the template, which stores it for you until
 * someone wants to use it. You can have any number of sections, and
 * you don't have to have a menu section. You don't even need a "user"
 * section, but then data added by a page handler function will not
 * be displayed.
 */
typedef struct html_template_tag* html_template;


/* HTML Functions */
html_buffer html_buffer_new(void);
void html_buffer_free(html_buffer b);
void html_buffer_set_template(html_buffer b, html_template t);
status_t html_add(html_buffer b, const char* s);
status_t html_printf(html_buffer b, size_t cb, const char* fmt, ...);

/* Sends the generated html to the client and will return the
 * appropriate returncode (e.g. 200 OK) to the client as well.
 * This function will also free the html_buffer object.
 */
int html_done(html_buffer b, http_response response, int returncode);

status_t html_anchor(html_buffer b, const char* url, const char* text);
status_t html_address(html_buffer b, const char* s);
status_t html_address_start(html_buffer b);
status_t html_address_end(html_buffer b);
status_t html_base(html_buffer b, const char* url);
status_t html_big(html_buffer b, const char* s);
status_t html_blockquote_start(html_buffer b, const char* url);
status_t html_blockquote_end(html_buffer b);
status_t html_body_start(html_buffer b);
status_t html_body_end(html_buffer b);
status_t html_bold(html_buffer b, const char* s);
status_t html_br(html_buffer b);
status_t html_button(html_buffer b, const char* name, const char* type, const char* value, const char* onfocus, const char* onblur);

status_t html_table_start(html_buffer b, size_t ncol);
status_t html_table_end(html_buffer b);
status_t html_th(html_buffer b, const char* s);
status_t html_td(html_buffer b, const char* s);
status_t html_tr_start(html_buffer b);
status_t html_tr_end(html_buffer b);
status_t html_strong(html_buffer b, const char* s);
status_t html_italic(html_buffer b, const char* s);
status_t html_slant(html_buffer b, const char* s);
status_t html_em(html_buffer b, const char* s);
status_t html_strong(html_buffer b, const char* s);
status_t html_dfn(html_buffer b, const char* s);
status_t html_code(html_buffer b, const char* s);
status_t html_samp(html_buffer b, const char* s);
status_t html_kbd(html_buffer b, const char* s);
status_t html_var(html_buffer b, const char* s);
status_t html_cite(html_buffer b, const char* s);
status_t html_abbr(html_buffer b, const char* s);
status_t html_acronym(html_buffer b, const char* s);
status_t html_small(html_buffer b, const char* s);

/* Lists */
/*@{*/
status_t html_dl_start(html_buffer b);
status_t html_dl_end(html_buffer b);
status_t html_dt(html_buffer b, const char* s);
status_t html_dd(html_buffer b, const char* s);

status_t html_ol_start(html_buffer b);
status_t html_ol_end(html_buffer b);

status_t html_ul_start(html_buffer b);
status_t html_ul_end(html_buffer b);

status_t html_li(html_buffer b, const char* s);

/*@}*/

status_t html_del(html_buffer b, const char* s);
status_t html_ins(html_buffer b, const char* s);

status_t html_p(html_buffer b, const char* s);
status_t html_p_start(html_buffer b);
status_t html_p_end(html_buffer b);

/* Headings */
/*@{*/
status_t html_h1(html_buffer b, const char* s);
status_t html_h2(html_buffer b, const char* s);
status_t html_h3(html_buffer b, const char* s);
status_t html_h4(html_buffer b, const char* s);
status_t html_h5(html_buffer b, const char* s);
status_t html_h6(html_buffer b, const char* s);
/*@}*/

status_t html_head_start(html_buffer b);
status_t html_head_end(html_buffer b);
status_t html_title(html_buffer b, const char* s);

status_t html_hr(html_buffer b);
status_t html_html_start(html_buffer b);
status_t html_html_end(html_buffer b);

status_t html_img(html_buffer b, const char* url, const char* alt, size_t height, size_t width);

status_t html_label(html_buffer b, const char* _for, const char* text);
status_t html_meta(html_buffer b, const char* s);
status_t html_q(html_buffer b, const char* s);
status_t html_samp(html_buffer b, const char* s);
status_t html_sub(html_buffer b, const char* s);
status_t html_sup(html_buffer b, const char* s);

/* Combo boxes */
/*@{*/
status_t html_select_start(html_buffer b);
status_t html_select_end(html_buffer b);
status_t html_option(html_buffer b, int selected, const char* value, const char* text);
status_t html_optgroup_start(html_buffer b, const char* label);
status_t html_optgroup_end(html_buffer b);
/*@}*/

status_t html_style_start(html_buffer b, const char* type);
status_t html_style_end(html_buffer b);
status_t html_tt(html_buffer b, const char* s);


/* Menu functions */
html_menu html_menu_new(void);
void html_menu_free(html_menu m);

status_t html_menu_set_text(html_menu m, const char* s);
status_t html_menu_set_image(html_menu m, const char* s);
status_t html_menu_set_hover_image(html_menu m, const char* s);
status_t html_menu_set_link(html_menu m, const char* s);
status_t html_menu_add_menu(html_menu m, html_menu submenu);

const char* html_menu_get_text(html_menu m);
const char* html_menu_get_image(html_menu m);
const char* html_menu_get_hover_image(html_menu m);
const char* html_menu_get_link(html_menu m);
int html_menu_render(html_menu m, cstring buffer);

/* @defgroup html_template template and template repository functions */
/*
 * Create a new template.
 */
html_template html_template_new(void);
void html_template_free(html_template t);

/*
 * Add a layout to the template.
 * A layout is just a string describing, in HTML, how the page should
 * look like. The string should contain special directives to identify
 * where the contents of the different sections should be printed.
 * The following directives exist:
 * %S Section code should be added here. The code for the first section
 *    will be added wherever the first %S is found, and so forth.
 *
 * %H Head. Code that belongs to the <HEAD></HEAD> tag will be added here.
 *    The html_buffer object as two functions, html_head_begin() and
 *    html_head_end(). The html_buffer object will store everything
 *    added to the buffer between html_head_begin() and html_head_end()
 *    to a special buffer. This buffer will later be inserted wherever
 *    the %H directive is found.
 *
 * An example:
 * "<html>%H<body><table><tr><td>%S</td></tr><tr><td>%S</td></tr></table>"
 * "</body></html>"
 */
void html_template_set_menu(html_template t, html_menu m);
int html_template_set_layout(html_template t, const char* s);
int html_template_add_section(html_template t, html_section s);
int html_template_add_user_section(html_template t);

int html_template_repository_add(const char* name, html_template t);
html_buffer html_template_repository_use(const char* name);
status_t html_template_send(
    html_template t,
    http_response response,
    const char* headcode,
    const char* usercode);

/*
 * We don't want valgrind to report memleaks, so this function
 * empties the repository.
 */
void html_template_repository_empty(void);

html_section html_section_new(void);
int html_section_set_name(html_section s, const char* v);
int html_section_set_code(html_section s, const char* v);
const char* html_section_get_code(html_section s);
const char* html_section_get_name(html_section s);
void html_section_free(html_section p);

#ifdef __cplusplus
}
#endif

#endif /* guard */
