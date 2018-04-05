SUBDIRS=tools meta http imap apps 
LDADD=-lpthread -L.
#AUTOMAKE_OPTIONS=subdir-objects

AM_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT\
	-I/usr/local/lib\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -ggdb -Wall -pedantic -Wno-long-long  -Wno-inline

# OpenSSL 1.1 is here
AM_LDFLAGS=-L/usr/local/lib

EXTRA_DIST=COPYING INSTALL AUTHORS NEWS README TODO
