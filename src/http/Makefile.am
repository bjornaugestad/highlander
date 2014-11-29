# The main target
lib_LTLIBRARIES=libhighlander.la libhighlanderd.la

include_HEADERS=\
	rfc1738.h\
	highlander.h\
	meta_html.h

noinst_HEADERS=\
	internals.h

libhighlander_la_LDFLAGS=-release $(PACKAGE_VERSION) -version-info 0:0:0
libhighlanderd_la_LDFLAGS=-release $(PACKAGE_VERSION) -version-info 0:0:0

libhighlander_la_SOURCES=\
	rfc1738.c\
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

libhighlanderd_la_SOURCES=$(libhighlander_la_SOURCES)

# Flags.
# -D_BSD_SOURCE is to get the prototype for strerror_r.
# _SVID_SOURCE will do the same thing.

libhighlander_la_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT -pthread\
	-I$(srcdir)\
	-I$(srcdir)/../meta/\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

libhighlanderd_la_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT -pthread\
	-I$(srcdir)\
	-I$(srcdir)/../meta/\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O0 -ggdb -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

if WITH_VALGRIND
libhighlander_la_CFLAGS+=-DWITH_VALGRIND=1 
endif

