bin_PROGRAMS=httpd

httpd_SOURCES=httpd.c

AM_CPPFLAGS=-I$(top_srcdir)/http/src -I$(top_srcdir)/meta/src

httpd_CFLAGS=\
	-Winline -Wshadow -std=gnu2x \
	-Wshadow\
	-Wformat-overflow=2 -Woverflow -Wshift-overflow=2 -Wstrict-overflow=5\
	-Wextra -Wall -pedantic -W -Werror -Wno-long-long -pthread -DNDEBUG -O3

httpd_LDFLAGS=-L$(top_srcdir)/meta/src -L$(top_srcdir)/http/src
httpd_LDADD=-lhighlander -lmeta -lpthread -lssl -lcrypto

