#include <stdio.h>
#include <assert.h>

#include <highlander.h>
#include <meta_filecache.h>

#include <httpcache.h>

int show_file_list(http_response page, const char* desc, list lst)
{
    list_iterator i;
    char buf[100];
    struct tm t;

    const char* table_start = 
        "<table columns='2' border='1' borderwidth='2'>\n"
        "<th>File name</th>\n"
        "<th>Alias/URI</th>\n"
        "<th>Size</th>\n"
        "<th>Last modified</th>\n";

    const char* table_end =
        "</table>\n";

    response_p(page, desc);

    response_add(page, table_start);

    for (i = list_first(lst); !list_end(i); i = list_next(i)) {
        fileinfo fi = list_get(i);
        const struct stat *pst = fileinfo_stat(fi);

        verbose(3, "Adding table td for file %s\n", fileinfo_alias(fi));
        response_add(page, "<tr>\n");
        response_td(page, fileinfo_name(fi));
        response_td(page, fileinfo_alias(fi));
        sprintf(buf, "%d", (int)pst->st_size);
        response_td(page, buf);

        localtime_r(&pst->st_mtime, &t);
        asctime_r(&t, buf);
        response_td(page, buf);
        response_add(page, "</tr>\n");
    }
    response_add(page, table_end);
    return 1;
}

static int add_menubar(http_response page, int pageid)
{
static struct {
    int pageid;
    const char* id;
    const char* href;
    const char* text;
    const char* title;
} items[] = {
    { PAGE_CACHE,	"id",  "/cache", "cache", "Show cache info" },
    { PAGE_DISK,	"id",  "/disk", "disk", "Show disk info" },
    { PAGE_STATS, 	"id",  "/stats", "statistics", "Show statistics about the web cache" },
    { PAGE_CONFIGFILE,"id",  "/configuration", "Configuration", "View the configuration file in use" },
};

    size_t cb, i, nelem = sizeof items / sizeof *items;
    const char* formatstring = "<li id='%s' class='%s'>\n\t<a href='%s' title='%s'>%s</a>\n</li>\n";

    response_add(page, "<ul id='menulist'>\n");
    for (i = 0; i < nelem; i++) {
        const char* sel = "plain";
        if (items[i].pageid == pageid) 
            sel = "selected";

        cb  = strlen(items[i].id) 
            + strlen(items[i].href) 
            + strlen(items[i].text) 
            + strlen(items[i].title) 
            + strlen(formatstring)
            + strlen(sel)
            + 30; /* for safety reasons */
        response_printf(page, cb, formatstring, 
            items[i].id,
            sel,
            items[i].href,
            items[i].title,
            items[i].text,
            items[i].id);
        response_add_char(page, '\n');
    }

    response_add(page, "</ul>\n");

    /* That's the main tabs. Now throw in a second ul with one item, /about, 
     * and place it on the line below.
     */

    const char* about = "<ul id='aboutline'><li><a href='/about' title='About the Highlander Web Cache'>about</a></li></ul>";
    response_add(page, about);

    return 1;

}

/*
 * Adds everything from <HTML> to <BODY>. title will be added 
 * between <title> and </title> 
 */
int add_page_start(http_response page, int pageid)
{
    const char* html = 
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Strict//EN\">\n"
        "<html>\n"
        "<head>\n"
        "<meta http-equiv='Content-Type' content='text/html'>\n"
        "<link href='/webcache_styles.css' rel=stylesheet type='text/css'>"
        "<title>The Highlander web cache\n</title>\n"
        "</head>\n"
        "<body>"
        "<a href='http://www.metasystems.no'>"
        "<img border=0 src='/webcache_logo.gif'>"
        "</a><br>"
        ;

    response_add(page, html);
    add_menubar(page, pageid);
    return 1;
}

/* All pages have a common end so call this function for all page handlers. 
 * It will add "</body></html>" so don't add html to the page after calling
 * this function. */
int add_page_end(http_response page, const char* msg)
{
    const char* html = 
    "\n<hr>\n"
#ifdef PACKAGE_VERSION
    "The Highlander Web Cache, version " PACKAGE_VERSION ".\n"
#endif
    "</body>\n";

    const char*tail = "</html>\n";

    response_add(page, html);
    if (msg != NULL && *msg != '\0')
        response_js_messagebox(page, msg);
	return response_add(page, tail) == success ? 1 : 0;
}

