bin_PROGRAMS=webcached
EXTRA_DIST= webcached.conf

webcached_SOURCES=\
	webcache_styles.css\
	webcache_logo.gif\
	httpcache.h\
	highcached.c \
	show_about.c\
	find_files.c\
	show_disk.c\
	show_cache.c\
	show_configuration.c\
	handle_requests.c\
	html_utilities.c\
	statpack.c

webcached_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT -pthread\
	-I$(srcdir)\
	-I$(srcdir)/webcache\
	-I$(top_srcdir)/meta/src\
	-I$(top_srcdir)/http/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast


webcached_LDADD=-lhighlander -lmeta -lpthread
webcached_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src

# Convert the GIF logo to C source code
%.c : %.gif
	../../tools/bin2c -bt image/gif -i httpcache.h -c $*.c $<

%.c : %.css
	../../tools/bin2c -bt text/css -i httpcache.h -c $*.c $<

CLEANFILES=webcache_logo.c webcache_styles.c

BUILT_SOURCES=\
	webcache_logo.c webcache_styles.c

nodist_webcached_SOURCES=webcache_logo.c webcache_styles.c

