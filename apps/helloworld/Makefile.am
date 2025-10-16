bin_PROGRAMS=helloworld

helloworld_SOURCES=main.c

helloworld_CFLAGS= -I$(top_srcdir)/http/src -I$(top_srcdir)/meta/src
helloworld_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
helloworld_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread

# See if we can use a script to check this program
TESTS=helloworld_check
