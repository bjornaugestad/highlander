bin_PROGRAMS=helloworld

helloworld_SOURCES=main.c

AM_CPPFLAGS=-I$(top_distdir)/http/src -I$(top_distdir)/meta/src
AM_CFLAGS=-p -pg

helloworld_CFLAGS=\
	-Winline -Wshadow -std=gnu2x \
	-fsanitize=address,undefined,leak\
	-Wshadow\
	-I/usr/local/lib\
	-I$(top_srcdir)/http/src\
	-I$(top_srcdir)/meta/src\
	-Wformat-overflow=2 -Woverflow -Wshift-overflow=2 -Wstrict-overflow=5\
	-Wextra -Wall -pedantic -W -Werror -Wno-long-long -pthread -O0 -ggdb

helloworld_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
helloworld_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread

# See if we can use a script to check this program
TESTS=helloworld_check
