bin_PROGRAMS=webclient

webclient_SOURCES=main.c

webclient_CFLAGS=-I$(top_srcdir)/http/src -I$(top_srcdir)/meta/src
webclient_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
webclient_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread

