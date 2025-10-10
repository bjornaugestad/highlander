#AUTOMAKE_OPTIONS=subdir-objects

# The main target
lib_LIBRARIES=libhighlander.a libhighlanderd.a

include_HEADERS=\
	rfc1738.h\
	highlander.h\
	meta_html.h

noinst_HEADERS=\
	internals.h

libhighlander_a_SOURCES=\
	http.c\
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
	http_client.c\
	parse_http.c\
	parse_time.c\
	request.c\
	response.c\
	general_header.c\
	entity_header.c\
	send_status_code.c

libhighlanderd_a_SOURCES=$(libhighlander_a_SOURCES)

# Flags.
# -D_BSD_SOURCE is to get the prototype for strerror_r.
# _SVID_SOURCE will do the same thing.

libhighlander_a_CFLAGS=\
	 -std=gnu2x -pthread\
	-DNDEBUG\
	-I$(srcdir)\
	-I$(top_srcdir)/meta/src\
	-ffunction-sections -fdata-sections\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -Wextra -Wextra -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

libhighlanderd_a_CFLAGS=\
	 -std=gnu2x -pthread\
	-I$(srcdir)\
	-I$(top_srcdir)/meta/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O0 -Wextra -ggdb -Wextra -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

TESTS=\
	http_server_check\
	http_client_check\
	attribute_check\
	rfc1738_check\
	cookie_check

check_PROGRAMS=\
	http_server_check\
	http_client_check\
	attribute_check\
	cookie_check\
	rfc1738_check

attribute_check_SOURCES=attribute.c
attribute_check_CFLAGS=-DCHECK_ATTRIBUTE $(AM_CFLAGS)\
	-I$(top_srcdir)/meta/src
attribute_check_LDADD=-lasan -lmeta -lssl -lcrypto -lpthread
attribute_check_LDFLAGS=-L$(top_srcdir)/meta/src

cookie_check_SOURCES=cookies.c
cookie_check_CFLAGS=-DCHECK_COOKIE $(AM_CFLAGS)\
	-I$(top_srcdir)/meta/src

cookie_check_LDADD=-lasan -lhighlander -lmeta -lssl -lcrypto -lpthread 
cookie_check_LDFLAGS=-L$(top_srcdir)/http/src -L$(top_srcdir)/meta/src

rfc1738_check_SOURCES=rfc1738.c
rfc1738_check_CFLAGS=-DCHECK_RFC1738 $(AM_CFLAGS)

http_server_check_SOURCES=\
	http_server.c\
	http_client.c\
	http.c\
	request.c\
	response.c\
	general_header.c\
	entity_header.c\
	cookies.c\
	parse_http.c\
	parse_time.c\
	highlander.c\
	dynamic_page.c\
	rfc1738.c\
	send_status_code.c\
	attribute.c
http_server_check_CFLAGS=-DCHECK_HTTP_SERVER $(AM_CFLAGS)\
	-I$(top_srcdir)/meta/src -O0 -ggdb
http_server_check_LDADD= -lasan -lmeta -lssl -lcrypto -lpthread
http_server_check_LDFLAGS=-L$(top_srcdir)/meta/src

http_client_check_SOURCES=\
	http_client.c\
	http.c\
	request.c\
	response.c\
	general_header.c\
	entity_header.c\
	cookies.c\
	parse_http.c\
	parse_time.c\
	dynamic_page.c\
	rfc1738.c\
	send_status_code.c\
	attribute.c
http_client_check_CFLAGS=-DCHECK_HTTP_CLIENT $(AM_CFLAGS)\
	-I$(top_srcdir)/meta/src -O0 -ggdb
http_client_check_LDADD= -lasan -lmeta -lssl -lcrypto -lpthread
http_client_check_LDFLAGS=-L$(top_srcdir)/meta/src
