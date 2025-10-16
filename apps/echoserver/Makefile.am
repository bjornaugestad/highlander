bin_PROGRAMS=echoserver

echoserver_SOURCES=main.c

AM_CPPFLAGS=-I$(top_srcdir)/http/src -I$(top_srcdir)/meta/src
echoserver_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
echoserver_LDADD=-lhighlander -lmeta -lpthread -lssl -lcrypto

TESTS=echoserver_check
