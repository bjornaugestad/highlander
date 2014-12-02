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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <cstring.h>
#include <meta_html.h>

struct html_buffer_tag {
	html_template template;
	cstring buffer;
};

int html_printf(html_buffer b, size_t cb, const char* fmt, ...)
{
	char *buf;
	va_list ap;
	int rc;

	assert(b != NULL);
	assert(cb > 0);
	assert(fmt != NULL);

	if ((buf = malloc(cb + 1)) == NULL)
		return 0;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);


	rc = cstring_concat(b->buffer, buf);
	free(buf);
	return rc;
}

int html_add(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);
	return cstring_concat(b->buffer, s);
}

html_buffer html_buffer_new(void)
{
	html_buffer b;

	if ((b = malloc(sizeof *b)) == NULL)
		;
	else if((b->buffer = cstring_new()) == NULL) {
		free(b);
		b = NULL;
	}

	return b;
}

void html_buffer_free(html_buffer b)
{
	if (b != NULL) {
		cstring_free(b->buffer);
		free(b);
	}
}

int html_anchor(html_buffer b, const char* url, const char* text)
{
	size_t cb;
	const char* template = "<a href=\"%s\">%s</a>";

	assert(b != NULL);
	assert(url != NULL);
	assert(text != NULL);
	assert(strlen(url) != 0);
	assert(strlen(text) != 0);

	/* How many bytes do we need? This computation includes %s twice
	 * and therefore returns (actual_value + 4). That's ok.
	 */
	cb = strlen(url) + strlen(text) + strlen(template) + 1;

	return cstring_printf(b->buffer, cb, template, url, text);
}

int html_address_start(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "<address>");
}

int html_address_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</address>");
}

int html_address(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);
	return cstring_concat3(b->buffer, "<address>", s, "</address>");
}

int html_base(html_buffer b, const char* url)
{
	assert(b != NULL);
	assert(url != NULL);
	(void)b;
	(void)url;

	return 0;
}

int html_big(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<big>", s, "</big>");
}

int html_blockquote_start(html_buffer b, const char* url)
{
	assert(b != NULL);
	assert(url != NULL);
	assert(strlen(url) > 0);

	return cstring_concat3(b->buffer, "<blockquote cite ='", url, "'>");
}

int html_blockquote_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</blockquote>");
}

int html_body_start(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<body>");
}

int html_body_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</body>");
}

int html_bold(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<b>", s, "</b>");
}

int html_br(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<br>");
}

int html_button(
	html_buffer b,
	const char* name,
	const char* type,
	const char* value,
	const char* onfocus,
	const char* onblur)
{
	assert(b != NULL);

	(void)b;
	(void)name;
	(void)type;
	(void)value;
	(void)onfocus;
	(void)onblur;
	assert(0 && "Implement me");
	return 0;
}


int html_table_start(html_buffer b, size_t ncol)
{
	assert(b != NULL);
	if (ncol > 0)
		return cstring_printf(
			b->buffer,
			100,
			"<table columns='%lu'>",
			(unsigned long)ncol);
	else
		return cstring_concat(b->buffer, "<table>");

}

int html_table_end(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "</table>");
}

int html_th(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<th>", s, "</th>");
}

int html_td(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<td>", s, "</td>");
}

int html_tr_start(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "<tr>");
}

int html_tr_end(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "</tr>");
}

int html_strong(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<strong>", s, "</strong>");
}

int html_italic(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<italic>", s, "</italic>");
}

int html_slant(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<slant>", s, "</slant>");
}

int html_em(html_buffer b, const char* s)
{
	assert(s != NULL);
	assert(b != NULL);

	return cstring_concat3(b->buffer, "<em>", s, "</em>");
}

int html_dfn(html_buffer b, const char* s)
{
	assert(s != NULL);
	assert(b != NULL);

	return cstring_concat3(b->buffer, "<dfn>", s, "</dfn>");
}

int html_code(html_buffer b, const char* s)
{
	assert(s != NULL);
	assert(b != NULL);

	return cstring_concat3(b->buffer, "<code>", s, "</code>");
}

int html_samp(html_buffer b, const char* s)
{
	assert(s != NULL);
	assert(b != NULL);

	return cstring_concat3(b->buffer, "<samp>", s, "</samp>");
}

int html_kbd(html_buffer b, const char* s)
{
	assert(s != NULL);
	assert(b != NULL);

	return cstring_concat3(b->buffer, "<kbd>", s, "</kbd>");
}

int html_var(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<var>", s, "</var>");
}

int html_cite(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<cite>", s, "</cite>");
}

int html_abbr(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<abbr>", s, "</abbr>");
}

int html_acronym(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<acronym>", s, "</acronym>");
}

int html_small(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<small>", s, "</small>");
}

int html_dl_start(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "<dl>");
}

int html_dl_end(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "</dl>");
}

int html_dt(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<dt>", s, "</dt>");
}

int html_dd(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<dd>", s, "</dd>");
}


int html_ol_start(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<ol>");
}

int html_ol_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</ol>");
}

int html_ul_start(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<ul>");

	return 0;
}

int html_ul_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</ul>");
}

int html_li(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<li>", s, "</li>");
}


int html_del(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<del>", s, "</del>");
}

int html_ins(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<ins>", s, "</ins>");
}

int html_p(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<p>", s, "</p>");
}

int html_p_start(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<p>");
}

int html_p_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</p>");
}

int html_h1(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<h1>", s, "</h1>");
}

int html_h2(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<h2>", s, "</h2>");
}

int html_h3(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<h3>", s, "</h3>");
}

int html_h4(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<h4>", s, "</h4>");
}

int html_h5(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<h5>", s, "</h5>");
}

int html_h6(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<h6>", s, "</h6>");
}

int html_head_start(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "<head>");
}

int html_head_end(html_buffer b)
{
	assert(b != NULL);

	return cstring_concat(b->buffer, "</head>");
}

int html_title(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<title>", s, "</title>");
}


int html_hr(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<hr>");
}

int html_html_start(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<html>");
}

int html_html_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</html>");
}

int html_img(
	html_buffer b,
	const char* url,
	const char* alt,
	size_t height,
	size_t width)
{
	assert(b != NULL);
	assert(url != NULL);
	assert(strlen(url) > 0);

	if (!cstring_concat3(b->buffer, "<img src='", url, "'"))
		return 0;

	if (alt != NULL && strlen(alt) > 0) {
		if (!cstring_concat3(b->buffer, " alt='", alt, "'"))
			return 0;
	}

	if (height > 0) {
		if (!cstring_printf(b->buffer, 100, " height='%lu'", (unsigned long)height))
			return 0;
	}

	if (width > 0) {
		if (!cstring_printf(b->buffer, 100, " width='%lu'", (unsigned long)width))
			return 0;
	}


	return cstring_concat(b->buffer, ">");
}

int html_label(html_buffer b, const char* _for, const char* text)
{
	size_t cb;
	const char* template = "<label for='%s'>%s</label>";

	assert(b != NULL);
	assert(_for != NULL);
	assert(text != NULL);
	assert(strlen(_for) > 0);
	assert(strlen(text) > 0);

	cb = strlen(template) + strlen(_for) + strlen(text) + 1;
	return cstring_printf(b->buffer, cb, template, _for, text);
}

int html_meta(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);
	(void)b;
	(void)s;

	return 0;
}

int html_q(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<q>", s, "</q>");
}

int html_sub(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<sub>", s, "</sub>");
}

int html_sup(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);

	return cstring_concat3(b->buffer, "<sup>", s, "</sup>");
}

int html_select_start(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "<select>");
}

int html_select_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</select>");
}

int html_option(
	html_buffer b,
	int selected,
	const char* value,
	const char* text)
{
	assert(b != NULL);
	assert(text != NULL);
	assert(strlen(text) > 0);

	if (!cstring_concat(b->buffer, "<option"))
		return 0;

	if (selected == 1) {
		if (!cstring_concat(b->buffer, " selected "))
			return 0;
	}

	if (value != NULL && strlen(value) > 0) {
		if (!cstring_concat3(b->buffer, " value='", value, "'"))
			return 0;
	}

	return cstring_concat3(b->buffer, ">", text, "</option>");
}

int html_optgroup_start(html_buffer b, const char* label)
{
	assert(b != NULL);

	if (label != NULL)
		return cstring_printf(
			b->buffer,
			strlen(label) + 50,
			"<optgroup label=\"%s\">",
			label);
	else
		return cstring_concat(b->buffer, "<optgroup>");
}

int html_optgroup_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</optgroup>");
}

int html_style_start(html_buffer b, const char* type)
{
	assert(b != NULL);
	assert(type != NULL);
	assert(strlen(type) > 0);

	return cstring_concat3(b->buffer, "<style type='", type, ">");
}

int html_style_end(html_buffer b)
{
	assert(b != NULL);
	return cstring_concat(b->buffer, "</style>");
}

int html_tt(html_buffer b, const char* s)
{
	assert(b != NULL);
	assert(s != NULL);
	return cstring_concat3(b->buffer, "<tt>", s, "</tt>");
}

void html_buffer_set_template(html_buffer b, html_template t)
{
	assert(b != NULL);
	assert(t != NULL);

	b->template = t;
}

int html_done(html_buffer b, http_response response, int returncode)
{
	int rc;

	rc = html_template_send(b->template, response, "fix later", c_str(b->buffer));
	html_buffer_free(b);
	return rc == 1 ? returncode : HTTP_500_INTERNAL_SERVER_ERROR;
}

