# The main target
lib_LTLIBRARIES=libhighlander.la 

include_HEADERS=\
	cstring.h\
	meta_atomic.h\
	meta_sampler.h\
	meta_common.h\
	meta_ticker.h\
	meta_process.h\
	meta_slotbuf.h\
	meta_pool.h\
	meta_array.h\
	meta_list.h\
	meta_fifo.h\
	meta_socket.h\
	meta_membuf.h\
	meta_stack.h\
	meta_bitset.h\
	meta_misc.h\
	meta_cache.h\
	meta_configfile.h\
	meta_filecache.h\
	meta_stringmap.h\
	meta_map.h\
	meta_tsb.h\
	connection.h\
	tcp_server.h\
	meta_pair.h\
	rfc1738.h\
	meta_error.h\
	meta_wlock.h\
	threadpool.h\
	highlander.h\
	meta_html.h

noinst_HEADERS=\
	internals.h

libhighlander_la_LDFLAGS=-release $(PACKAGE_VERSION) -version-info 0:0:0
libhighlander_la_SOURCES=\
	meta_common.c\
	meta_sampler.c\
	meta_wlock.c\
	meta_ticker.c\
	meta_slotbuf.c\
	meta_map.c\
	meta_stack.c\
	meta_bitset.c\
	meta_process.c\
	meta_socket.c\
	meta_error.c\
	meta_tsb.c\
	tcp_server.c\
	connection.c\
	cstring.c\
	meta_membuf.c\
	meta_fifo.c\
	meta_pair.c\
	meta_array.c\
	meta_misc.c\
	meta_pool.c\
	threadpool.c\
	meta_configfile.c\
	rfc1738.c\
	meta_cache.c\
	meta_filecache.c\
	meta_list.c\
	meta_stringmap.c\
	attribute.c\
	cookies.c\
	dynamic_page.c\
	highlander.c\
	html_buffer.c\
	html_menu.c\
	html_section.c\
	html_template.c\
	html_template_repository.c\
	http_server.c\
	parse_http.c\
	parse_time.c\
	request.c\
	response.c\
	general_header.c\
	entity_header.c\
	send_status_code.c

# Flags.
# _XOPEN_SOURCE=600 is there to get rwlocks in pthread
# __POSIX_C_SOURCE=199506L ->
# -D_BSD_SOURCE is to get the prototype for strerror_r.
# _SVID_SOURCE will do the same thing.

libhighlander_a_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT -pthread\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -ggdb -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

if WITH_VALGRIND
libhighlander_a_CFLAGS+=-DWITH_VALGRIND=1 
endif

