bin_PROGRAMS=webclient

webclient_SOURCES=main.c

AM_CPPFLAGS=-I$(top_distdir)/http/src -I$(top_distdir)/meta/src
AM_CFLAGS=-p -pg

webclient_CFLAGS=\
	-Winline -Wshadow -std=gnu99 -D_GNU_SOURCE\
	-Wshadow\
	-I$(top_srcdir)/http/src\
	-I$(top_srcdir)/meta/src\
	-Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3

webclient_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
webclient_LDADD=-lhighlander -lmeta -lpthread

