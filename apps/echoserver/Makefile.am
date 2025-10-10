bin_PROGRAMS=echoserver

echoserver_SOURCES=main.c

AM_CPPFLAGS=-I$(top_srcdir)/http/src -I$(top_srcdir)/meta/src
AM_CFLAGS=-p -pg

echoserver_CFLAGS=\
	-Winline -Wshadow -std=gnu2x \
	-Wshadow\
	-Wformat-overflow=2 -Woverflow -Wshift-overflow=2 -Wstrict-overflow=5\
	-Wextra -Wall -pedantic -W -Werror -Wno-long-long -pthread 

echoserver_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
echoserver_LDADD=-lhighlander -lmeta -lpthread -lssl -lcrypto

TESTS=echoserver_check
