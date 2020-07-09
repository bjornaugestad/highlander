SUBDIRS=tools meta http apps 
LDADD=-lpthread -L.
#AUTOMAKE_OPTIONS=subdir-objects

AM_CFLAGS=\
	-D_GNU_SOURCE -std=gnu2x -D_REENTRANT\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -ggdb -Wextra -Wall -pedantic -Wno-long-long  -Wno-inline

EXTRA_DIST=COPYING INSTALL AUTHORS NEWS README TODO
