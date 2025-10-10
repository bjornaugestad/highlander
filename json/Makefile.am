lib_LIBRARIES=libjson.a

libjson_a_SOURCES=json.c
libjson_a_CFLAGS=-Wall -Wextra -I$(top_srcdir)/meta/src
include_HEADERS=json.h


TESTS=json_check
check_PROGRAMS=json_check

json_check_SOURCES=json.c
json_check_CFLAGS=-DJSON_CHECK -I $(top_srcdir)/meta/src -Wall -Wextra -Werror -pedantic -std=c99\
	-O0 -ggdb -D_XOPEN_SOURCE=700
json_check_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread
json_check_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src


	


