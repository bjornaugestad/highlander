SUBDIRS=tools meta http apps 
LDADD=-lpthread -L.
#AUTOMAKE_OPTIONS=subdir-objects

AM_CFLAGS=\
	-std=gnu2x -D_REENTRANT\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-fsanitize=address,undefined,leak\
	-Wformat-overflow=2 -Woverflow -Wshift-overflow=2 -Wstrict-overflow=5\
	-O2 -ggdb -Wextra -Wall -pedantic -Wno-long-long  -Wno-inline

EXTRA_DIST=COPYING INSTALL AUTHORS NEWS README TODO
