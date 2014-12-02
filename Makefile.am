SUBDIRS=tools src apps doc
LDADD=-lpthread -L.
AUTOMAKE_OPTIONS=subdir-objects

AM_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -ggdb -Wall -pedantic -Wno-long-long  -Wno-inline

EXTRA_DIST=COPYING INSTALL AUTHORS NEWS README TODO
