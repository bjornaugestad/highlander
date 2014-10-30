SUBDIRS=tools src apps doc
LDADD=-lpthread -L.
AUTOMAKE_OPTIONS=subdir-objects

TESTS=\
	sampler_check\
	bitset_check\
	fifo_check\
	list_check\
	wlock_check\
	stack_check\
	configfile_check\
	cstring_check\
	membuf_check\
	pool_check\
	array_check\
	rfc1738_check\
	cache_check\
	filecache_check\
	stringmap_check\
	miscfunc_check\
	tsb_check\
	pair_check\
	attribute_check\
	cookie_check

AM_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -ggdb -Wall -pedantic -Wno-long-long  -Wno-inline

check_PROGRAMS=\
	attribute_check\
	cookie_check\
	sampler_check\
	list_check\
	bitset_check\
	fifo_check\
	wlock_check\
	stack_check\
	configfile_check\
	cstring_check\
	membuf_check\
	pool_check\
	array_check\
	rfc1738_check\
	cache_check\
	filecache_check\
	stringmap_check\
	miscfunc_check\
	tsb_check\
	pair_check

attribute_check_SOURCES=src/attribute.c   src/cstring.c src/meta_common.c
attribute_check_CFLAGS=-DCHECK_ATTRIBUTE $(AM_CFLAGS)

cookie_check_SOURCES=src/cookies.c src/cstring.c
cookie_check_CFLAGS=-DCHECK_COOKIE $(AM_CFLAGS)
cookie_check_LDADD=-lhighlander -lpthread
cookie_check_LDFLAGS=-L.


list_check_SOURCES=src/meta_list.c src/meta_common.c
list_check_CFLAGS=-DLIST_CHECK $(AM_CFLAGS) 

bitset_check_SOURCES=src/meta_bitset.c src/meta_common.c
bitset_check_CFLAGS=-DBITSET_CHECK $(AM_CFLAGS) 

configfile_check_SOURCES=src/meta_configfile.c src/meta_common.c
configfile_check_CFLAGS=-DCHECK_CONFIGFILE $(AM_CFLAGS) 

cstring_check_SOURCES=src/cstring.c src/meta_common.c
cstring_check_CFLAGS=-DCHECK_CSTRING $(AM_CFLAGS)

membuf_check_SOURCES=src/meta_membuf.c src/meta_common.c
membuf_check_CFLAGS=-DCHECK_MEMBUF $(AM_CFLAGS)

pool_check_SOURCES=src/meta_pool.c src/meta_array.c src/meta_common.c
pool_check_CFLAGS=-DCHECK_POOL $(AM_CFLAGS)

array_check_SOURCES=src/meta_array.c src/meta_common.c
array_check_CFLAGS=-DCHECK_ARRAY $(AM_CFLAGS)

rfc1738_check_SOURCES=src/rfc1738.c
rfc1738_check_CFLAGS=-DCHECK_RFC1738 $(AM_CFLAGS)

cache_check_SOURCES=src/meta_cache.c src/meta_list.c src/meta_common.c
cache_check_CFLAGS=-DCHECK_CACHE $(AM_CFLAGS)

filecache_check_SOURCES=src/meta_filecache.c src/meta_stringmap.c src/meta_cache.c src/meta_list.c src/meta_common.c
filecache_check_CFLAGS=-DCHECK_FILECACHE $(AM_CFLAGS)

stringmap_check_SOURCES=src/meta_stringmap.c src/meta_list.c src/meta_common.c
stringmap_check_CFLAGS=-DCHECK_STRINGMAP $(AM_CFLAGS)

miscfunc_check_SOURCES=src/meta_misc.c src/meta_common.c
miscfunc_check_CFLAGS=-DCHECK_MISCFUNC $(AM_CFLAGS)

pair_check_SOURCES=src/meta_pair.c src/meta_common.c
pair_check_CFLAGS=-DCHECK_PAIR $(AM_CFLAGS)

fifo_check_SOURCES=src/meta_fifo.c src/meta_wlock.c src/meta_common.c
fifo_check_CFLAGS=-DCHECK_FIFO $(AM_CFLAGS)

tsb_check_SOURCES=src/meta_tsb.c  src/meta_common.c
tsb_check_CFLAGS=-DCHECK_TSB $(AM_CFLAGS)

wlock_check_SOURCES=src/meta_wlock.c src/meta_common.c
wlock_check_CFLAGS=-DCHECK_WLOCK -O0 -ggdb -D_GNU_SOURCE -std=gnu99 -D_REENTRANT -I./src
#wlock_check_CFLAGS=-DCHECK_WLOCK -O0 -ggdb $(AM_CFLAGS)

stack_check_SOURCES=src/meta_stack.c src/meta_list.c src/meta_common.c
stack_check_CFLAGS=-DCHECK_STACK -O0 -ggdb $(AM_CFLAGS)

sampler_check_SOURCES=src/meta_sampler.c src/meta_common.c
sampler_check_CFLAGS=-DCHECK_SAMPLER $(AM_CFLAGS)
sampler_check_LDADD=-lpthread
sampler_check_LDFLAGS=-ggdb -O0

EXTRA_DIST=COPYING INSTALL AUTHORS NEWS README TODO


