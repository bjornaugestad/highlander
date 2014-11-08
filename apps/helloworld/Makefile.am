bin_PROGRAMS=helloworld

helloworld_SOURCES=main.c

AM_CPPFLAGS=-I$(top_srcdir)/src
AM_CFLAGS=-p -pg

helloworld_CFLAGS=\
	-Winline -Wshadow -std=gnu99 -D_GNU_SOURCE\
	-Wshadow\
	-Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3

helloworld_LDFLAGS=-L$(top_srcdir)/src
helloworld_LDADD=-lhighlander -lpthread

