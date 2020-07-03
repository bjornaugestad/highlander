bin_PROGRAMS=helloworld

helloworld_SOURCES=main.c

AM_CPPFLAGS=-I$(top_distdir)/http/src -I$(top_distdir)/meta/src
AM_CFLAGS=-p -pg

helloworld_CFLAGS=\
	-Winline -Wshadow -std=gnu2x -D_GNU_SOURCE\
	-Wshadow\
	-I/usr/local/lib\
	-I$(top_srcdir)/http/src\
	-I$(top_srcdir)/meta/src\
	-Wextra -Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3

helloworld_LDFLAGS=-L/usr/local/lib -L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
helloworld_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread

# See if we can use a script to check this program
TESTS=helloworld_check
