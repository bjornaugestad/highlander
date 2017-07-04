bin_PROGRAMS=metaftpd

metaftpd_SOURCES=metaftp.c about.html document.png folder.gif
nodist_metaftpd_SOURCES=document.c about.c folder.c pagehandlers.h
EXTRA_DIST=metaftp.conf

AM_CPPFLAGS=-I$(top_srcdir)/meta/src -I$(top_srcdir)/http/src
AM_CFLAGS=-p -pg -Werror

metaftpd_CFLAGS=\
	-Winline -Wshadow -std=gnu99 -D_GNU_SOURCE\
	-Wshadow\
	-Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3\
	-Wno-deprecated-declarations

metaftpd_LDFLAGS=-L$(top_srcdir)/http/src -L$(top_srcdir)/meta/src
metaftpd_LDADD=-lhighlander -lmetad -lssl -lcrypto -lpthread

# New version of bin2c, can now create a header in a separate run
# I'm gonna eat my own dog food and change this project accordingly :-)
pagehandlers.h: folder.gif document.png about.html
	../../tools/bin2c -bh $@ $+

%.c : %.gif
	../../tools/bin2c -bt image/gif -i pagehandlers.h -c $*.c $<

%.c : %.png
	../../tools/bin2c -bt image/png -i pagehandlers.h -c $*.c $<

%.c : %.html
	../../tools/bin2c -bt text/html -i pagehandlers.h -c $*.c $<

CLEANFILES=\
	about.c\
	document.c\
	folder.c\
	document.c\
	document.h\
	folder.h\
	pagehandlers.h

BUILT_SOURCES=about.c document.c folder.c document.c pagehandlers.h
