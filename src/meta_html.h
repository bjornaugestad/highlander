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

/**
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
int html_add(html_buffer b, const char* s);
int html_printf(html_buffer b, size_t cb, const char* fmt, ...);

/* Sends the generated html to the client and will return the 
 * appropriate returncode (e.g. 200 OK) to the client as well.
 * This function will also free the html_buffer object.
 */
int html_done(html_buffer b, http_response response, int returncode);

int html_anchor(html_buffer b, const char* url, const char* text);
int html_address(html_buffer b, const char* s);
int html_address_start(html_buffer b);
int html_address_end(html_buffer b);
int html_base(html_buffer b, const char* url);
int html_big(html_buffer b, const char* s);
int html_blockquote_start(html_buffer b, const char* url);
int html_blockquote_end(html_buffer b);
int html_body_start(html_buffer b);
int html_body_end(html_buffer b);
int html_bold(html_buffer b, const char* s);
int html_br(html_buffer b);
int html_button(html_buffer b, const char* name, const char* type, const char* value, const char* onfocus, const char* onblur);

int html_table_start(html_buffer b, size_t ncol);
int html_table_end(html_buffer b);
int html_th(html_buffer b, const char* s);
int html_td(html_buffer b, const char* s);
int html_tr_start(html_buffer b);
int html_tr_end(html_buffer b);
int html_strong(html_buffer b, const char* s);
int html_italic(html_buffer b, const char* s);
int html_slant(html_buffer b, const char* s);
int html_em(html_buffer b, const char* s);
int html_strong(html_buffer b, const char* s);
int html_dfn(html_buffer b, const char* s);
int html_code(html_buffer b, const char* s);
int html_samp(html_buffer b, const char* s);
int html_kbd(html_buffer b, const char* s);
int html_var(html_buffer b, const char* s);
int html_cite(html_buffer b, const char* s);
int html_abbr(html_buffer b, const char* s);
int html_acronym(html_buffer b, const char* s);
int html_small(html_buffer b, const char* s);

/** Lists */
/*@{*/
int html_dl_start(html_buffer b);
int html_dl_end(html_buffer b);
int html_dt(html_buffer b, const char* s);
int html_dd(html_buffer b, const char* s);

int html_ol_start(html_buffer b);
int html_ol_end(html_buffer b);

int html_ul_start(html_buffer b);
int html_ul_end(html_buffer b);

int html_li(html_buffer b, const char* s);

/*@}*/

int html_del(html_buffer b, const char* s);
int html_ins(html_buffer b, const char* s);

int html_p(html_buffer b, const char* s);
int html_p_start(html_buffer b);
int html_p_end(html_buffer b);

/** Headings */
/*@{*/
int html_h1(html_buffer b, const char* s);
int html_h2(html_buffer b, const char* s);
int html_h3(html_buffer b, const char* s);
int html_h4(html_buffer b, const char* s);
int html_h5(html_buffer b, const char* s);
int html_h6(html_buffer b, const char* s);
/*@}*/

int html_head_start(html_buffer b);
int html_head_end(html_buffer b);
int html_title(html_buffer b, const char* s);

int html_hr(html_buffer b);
int html_html_start(html_buffer b);
int html_html_end(html_buffer b);

int html_img(html_buffer b, const char* url, const char* alt, size_t height, size_t width);

int html_label(html_buffer b, const char* _for, const char* text);
int html_meta(html_buffer b, const char* s);
int html_q(html_buffer b, const char* s);
int html_samp(html_buffer b, const char* s);
int html_sub(html_buffer b, const char* s);
int html_sup(html_buffer b, const char* s);

/** Combo boxes */
/*@{*/
int html_select_start(html_buffer b);
int html_select_end(html_buffer b);
int html_option(html_buffer b, int selected, const char* value, const char* text);
int html_optgroup_start(html_buffer b, const char* label);
int html_optgroup_end(html_buffer b);
/*@}*/

int html_style_start(html_buffer b, const char* type);
int html_style_end(html_buffer b);
int html_tt(html_buffer b, const char* s);


/** Menu functions */
html_menu html_menu_new(void);
void html_menu_free(html_menu m);

int html_menu_set_text(html_menu m, const char* s);
int html_menu_set_image(html_menu m, const char* s);
int html_menu_set_hover_image(html_menu m, const char* s);
int html_menu_set_link(html_menu m, const char* s);
int html_menu_add_menu(html_menu m, html_menu submenu);

const char* html_menu_get_text(html_menu m);
const char* html_menu_get_image(html_menu m);
const char* html_menu_get_hover_image(html_menu m);
const char* html_menu_get_link(html_menu m);
int html_menu_render(html_menu m, cstring buffer);

/** @defgroup html_template template and template repository functions */
/**
 * Create a new template. 
 */
html_template html_template_new(void);
void html_template_free(html_template t);

/**
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
int html_template_send(
	html_template t,
	http_response response,
	const char* headcode,
	const char* usercode);

/**
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

