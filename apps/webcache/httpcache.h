#ifndef HTTPCACHE_H
#define HTTPCACHE_H

#include <stddef.h>
#include <meta_list.h>
#include <highlander.h>
#include <cstring.h>


#include <meta_common.h>

/* This is a Common include file for all modules */


/* Our main pages have an id */
#define PAGE_STATS 1
#define PAGE_DISK 2
#define PAGE_CACHE 3
#define PAGE_CONFIGFILE 4
#define PAGE_ABOUT 5
#define PAGE_MAIN 6
#if 0
#define PAGE_NEW_FILES 2
#define PAGE_MODIFIED_FILES 3
#define PAGE_DELETED_FILES 4
#define PAGE_REFRESH_CACHE 5
#endif

list find_modified_files(const char* directories, cstring* patterns, size_t npatterns);
list find_new_files(const char* directories, cstring* patterns, size_t npatterns);
list find_deleted_files(const char* directories, cstring* patterns, size_t npatterns);

status_t walk_all_directories(const char* directories, cstring* patterns, size_t npatterns, list lst, int get_mimetype)
	__attribute__((warn_unused_result));


status_t find_files(const char* rootdir, const char* dirname, cstring* patterns, size_t npatterns, list lst, int get_mimetype)
	__attribute__((warn_unused_result));


/* HTML Utility functions */
status_t add_page_start(http_response page, int pageid)
	__attribute__((warn_unused_result));


/* Adds </body>opt_msg</html> where opt_msg is a string which will
 * be shown in a message box if opt_msg != NULL.
 */
status_t add_page_end(http_response page, const char* opt_msg)
	__attribute__((warn_unused_result));

status_t show_file_list(http_response page, const char* desc, list lst)
	__attribute__((warn_unused_result));


/* Callback function for the normal requests */
int handle_requests(const http_request req, http_response page);

/* The admin page handlers */
int handle_main(http_request req, http_response page);
int show_stats(http_request req, http_response page);
int show_configuration(http_request req, http_response page);
int show_disk(http_request req, http_response page);
int show_cache(http_request req, http_response page);
int show_about(http_request req, http_response page);

/* The statpack interface. Start and stop the sampler thread(s). */
int statpack_start(void);
int statpack_stop(void);

/* Image handler functions */
int show_webcache_logo_gif(http_request req, http_response page);
int show_webcache_styles_css(http_request request, http_response response);

#endif
