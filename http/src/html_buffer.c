/*
 * Copyright (C) 2001-2017 Bjorn Augestad, bjorn@augestad.online
 * All Rights Reserved. See COPYING for license details
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

status_t html_printf(html_buffer b, size_t cb, const char *fmt, ...)
{
    char *buf;
    va_list ap;
    status_t rc;

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

status_t html_add(html_buffer b, const char *s)
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
    else if ((b->buffer = cstring_new()) == NULL) {
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

status_t html_anchor(html_buffer b, const char *url, const char *text)
{
    const char *template = "<a href=\"%s\">%s</a>";

    assert(b != NULL);
    assert(url != NULL);
    assert(text != NULL);
    assert(strlen(url) != 0);
    assert(strlen(text) != 0);

    return cstring_printf(b->buffer, template, url, text);
}

status_t html_address_start(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "<address>");
}

status_t html_address_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</address>");
}

status_t html_address(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);
    return cstring_concat3(b->buffer, "<address>", s, "</address>");
}

status_t html_base(html_buffer b, const char *url)
{
    assert(b != NULL);
    assert(url != NULL);
    (void)b;
    (void)url;

    return 0;
}

status_t html_big(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<big>", s, "</big>");
}

status_t html_blockquote_start(html_buffer b, const char *url)
{
    assert(b != NULL);
    assert(url != NULL);
    assert(strlen(url) > 0);

    return cstring_concat3(b->buffer, "<blockquote cite ='", url, "'>");
}

status_t html_blockquote_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</blockquote>");
}

status_t html_body_start(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<body>");
}

status_t html_body_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</body>");
}

status_t html_bold(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<b>", s, "</b>");
}

status_t html_br(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<br>");
}

status_t html_button(
    html_buffer b,
    const char *name,
    const char *type,
    const char *value,
    const char *onfocus,
    const char *onblur)
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


status_t html_table_start(html_buffer b, size_t ncol)
{
    assert(b != NULL);
    if (ncol > 0)
        return cstring_printf(b->buffer,
            "<table columns='%lu'>",
            (unsigned long)ncol);
    else
        return cstring_concat(b->buffer, "<table>");

}

status_t html_table_end(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "</table>");
}

status_t html_th(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<th>", s, "</th>");
}

status_t html_td(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<td>", s, "</td>");
}

status_t html_tr_start(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "<tr>");
}

status_t html_tr_end(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "</tr>");
}

status_t html_strong(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<strong>", s, "</strong>");
}

status_t html_italic(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<italic>", s, "</italic>");
}

status_t html_slant(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<slant>", s, "</slant>");
}

status_t html_em(html_buffer b, const char *s)
{
    assert(s != NULL);
    assert(b != NULL);

    return cstring_concat3(b->buffer, "<em>", s, "</em>");
}

status_t html_dfn(html_buffer b, const char *s)
{
    assert(s != NULL);
    assert(b != NULL);

    return cstring_concat3(b->buffer, "<dfn>", s, "</dfn>");
}

status_t html_code(html_buffer b, const char *s)
{
    assert(s != NULL);
    assert(b != NULL);

    return cstring_concat3(b->buffer, "<code>", s, "</code>");
}

status_t html_samp(html_buffer b, const char *s)
{
    assert(s != NULL);
    assert(b != NULL);

    return cstring_concat3(b->buffer, "<samp>", s, "</samp>");
}

status_t html_kbd(html_buffer b, const char *s)
{
    assert(s != NULL);
    assert(b != NULL);

    return cstring_concat3(b->buffer, "<kbd>", s, "</kbd>");
}

status_t html_var(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<var>", s, "</var>");
}

status_t html_cite(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<cite>", s, "</cite>");
}

status_t html_abbr(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<abbr>", s, "</abbr>");
}

status_t html_acronym(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<acronym>", s, "</acronym>");
}

status_t html_small(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<small>", s, "</small>");
}

status_t html_dl_start(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "<dl>");
}

status_t html_dl_end(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "</dl>");
}

status_t html_dt(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<dt>", s, "</dt>");
}

status_t html_dd(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<dd>", s, "</dd>");
}


status_t html_ol_start(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<ol>");
}

status_t html_ol_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</ol>");
}

status_t html_ul_start(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<ul>");

    return 0;
}

status_t html_ul_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</ul>");
}

status_t html_li(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<li>", s, "</li>");
}


status_t html_del(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<del>", s, "</del>");
}

status_t html_ins(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<ins>", s, "</ins>");
}

status_t html_p(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<p>", s, "</p>");
}

status_t html_p_start(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<p>");
}

status_t html_p_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</p>");
}

status_t html_h1(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<h1>", s, "</h1>");
}

status_t html_h2(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<h2>", s, "</h2>");
}

status_t html_h3(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<h3>", s, "</h3>");
}

status_t html_h4(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<h4>", s, "</h4>");
}

status_t html_h5(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<h5>", s, "</h5>");
}

status_t html_h6(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<h6>", s, "</h6>");
}

status_t html_head_start(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "<head>");
}

status_t html_head_end(html_buffer b)
{
    assert(b != NULL);

    return cstring_concat(b->buffer, "</head>");
}

status_t html_title(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<title>", s, "</title>");
}


status_t html_hr(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<hr>");
}

status_t html_html_start(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<html>");
}

status_t html_html_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</html>");
}

status_t html_img(
    html_buffer b,
    const char *url,
    const char *alt,
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
        if (!cstring_printf(b->buffer, " height='%lu'", (unsigned long)height))
            return 0;
    }

    if (width > 0) {
        if (!cstring_printf(b->buffer, " width='%lu'", (unsigned long)width))
            return 0;
    }


    return cstring_concat(b->buffer, ">");
}

status_t html_label(html_buffer b, const char *_for, const char *text)
{
    const char *template = "<label for='%s'>%s</label>";

    assert(b != NULL);
    assert(_for != NULL);
    assert(text != NULL);
    assert(strlen(_for) > 0);
    assert(strlen(text) > 0);

    return cstring_printf(b->buffer, template, _for, text);
}

status_t html_meta(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);
    (void)b;
    (void)s;

    return 0;
}

status_t html_q(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<q>", s, "</q>");
}

status_t html_sub(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<sub>", s, "</sub>");
}

status_t html_sup(html_buffer b, const char *s)
{
    assert(b != NULL);
    assert(s != NULL);

    return cstring_concat3(b->buffer, "<sup>", s, "</sup>");
}

status_t html_select_start(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "<select>");
}

status_t html_select_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</select>");
}

status_t html_option(
    html_buffer b,
    int selected,
    const char *value,
    const char *text)
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

status_t html_optgroup_start(html_buffer b, const char *label)
{
    assert(b != NULL);

    if (label != NULL)
        return cstring_printf(b->buffer, "<optgroup label=\"%s\">", label);
    else
        return cstring_concat(b->buffer, "<optgroup>");
}

status_t html_optgroup_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</optgroup>");
}

status_t html_style_start(html_buffer b, const char *type)
{
    assert(b != NULL);
    assert(type != NULL);
    assert(strlen(type) > 0);

    return cstring_concat3(b->buffer, "<style type='", type, ">");
}

status_t html_style_end(html_buffer b)
{
    assert(b != NULL);
    return cstring_concat(b->buffer, "</style>");
}

status_t html_tt(html_buffer b, const char *s)
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
    status_t rc;

    rc = html_template_send(b->template, response, "fix later", c_str(b->buffer));
    html_buffer_free(b);
    return rc == success ? returncode : HTTP_500_INTERNAL_SERVER_ERROR;
}
