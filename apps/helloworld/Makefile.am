bin_PROGRAMS=helloworld

helloworld_SOURCES=main.c

AM_CPPFLAGS=-I$(srcdir)
AM_CFLAGS=-p -pg

helloworld_CFLAGS=\
	-Winline -Wshadow -std=gnu99 -D_GNU_SOURCE\
	-Wshadow\
	-Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3

helloworld_LDFLAGS=-L..
helloworld_LDADD=-lhighlander -lpthread

