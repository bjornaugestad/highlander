AUTOMAKE_OPTIONS=subdir-objects

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
	-I$(top_srcdir)/meta/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

libhighlanderd_la_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT -pthread\
	-I$(srcdir)\
	-I$(top_srcdir)/meta/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O0 -ggdb -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

if WITH_VALGRIND
libhighlander_la_CFLAGS+=-DWITH_VALGRIND=1 
endif

TESTS=\
	attribute_check\
	rfc1738_check\
	cookie_check

check_PROGRAMS=\
	attribute_check\
	cookie_check\
	rfc1738_check

attribute_check_SOURCES=attribute.c
attribute_check_CFLAGS=-DCHECK_ATTRIBUTE $(AM_CFLAGS)\
	-I$(top_srcdir)/meta/src
attribute_check_LDADD=-lmeta -lpthread
attribute_check_LDFLAGS=-L$(top_srcdir)/meta/src

cookie_check_SOURCES=cookies.c
cookie_check_CFLAGS=-DCHECK_COOKIE $(AM_CFLAGS)\
	-I$(top_srcdir)/meta/src

cookie_check_LDADD=-lhighlander -lmeta -lpthread
cookie_check_LDFLAGS=-L$(top_srcdir)/meta/src

rfc1738_check_SOURCES=rfc1738.c
rfc1738_check_CFLAGS=-DCHECK_RFC1738 $(AM_CFLAGS)


