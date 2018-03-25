bin_PROGRAMS=echoserver

echoserver_SOURCES=main.c

AM_CPPFLAGS=-I$(top_srcdir)/http/src -I$(top_srcdir)/meta/src
AM_CFLAGS=-p -pg

echoserver_CFLAGS=\
	-Winline -Wshadow -std=gnu99 -D_GNU_SOURCE\
	-Wshadow\
	-Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3

echoserver_LDFLAGS=-L/usr/local/lib -L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
echoserver_LDADD=-lhighlander -lmetad -lpthread -lssl -lcrypto

TESTS=echoserver_check
