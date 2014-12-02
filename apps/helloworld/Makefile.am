bin_PROGRAMS=helloworld

helloworld_SOURCES=main.c

AM_CPPFLAGS=-I$(top_distdir)/http/src -I$(top_distdir)/meta/src
AM_CFLAGS=-p -pg

helloworld_CFLAGS=\
	-Winline -Wshadow -std=gnu99 -D_GNU_SOURCE\
	-Wshadow\
	-Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3

helloworld_LDFLAGS=-L$(top_distdir)/meta/src -L$(top_distdir)/http/src
helloworld_LDADD=-lhighlander -lmeta -lpthread

