bin_PROGRAMS=httpd

httpd_SOURCES=httpd.c

httpd_CFLAGS=-I$(top_srcdir)/http/src -I$(top_srcdir)/meta/src
httpd_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
httpd_LDADD=-lhighlander -lmeta -lpthread -lssl -lcrypto

