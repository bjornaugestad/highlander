#AUTOMAKE_OPTIONS=subdir-objects

# The main target
lib_LIBRARIES=libmeta.a libmetad.a

include_HEADERS=\
	cstring.h\
	meta_atomic.h\
	meta_sampler.h\
	meta_common.h\
	meta_ticker.h\
	meta_process.h\
	meta_slotbuf.h\
	meta_pool.h\
	meta_array.h\
	meta_list.h\
	meta_fifo.h\
	miscssl.h\
	tcpsocket.h\
	sslsocket.h\
	gensocket.h\
	meta_membuf.h\
	meta_stack.h\
	meta_bitset.h\
	meta_misc.h\
	meta_cache.h\
	meta_configfile.h\
	meta_filecache.h\
	meta_regex.h\
	meta_stringmap.h\
	meta_map.h\
	connection.h\
	meta_pair.h\
	meta_error.h\
	meta_wlock.h\
	meta_convert.h\
	tcp_server.h\
	threadpool.h

libmeta_a_SOURCES=\
	meta_common.c\
	meta_sampler.c\
	meta_wlock.c\
	meta_ticker.c\
	meta_slotbuf.c\
	meta_map.c\
	meta_stack.c\
	meta_bitset.c\
	meta_process.c\
	miscssl.c\
	tcpsocket.c\
	sslsocket.c\
	gensocket.c\
	meta_error.c\
	tcp_server.c\
	connection.c\
	cstring.c\
	meta_membuf.c\
	meta_fifo.c\
	meta_pair.c\
	meta_array.c\
	meta_misc.c\
	meta_pool.c\
	threadpool.c\
	meta_configfile.c\
	meta_cache.c\
	meta_filecache.c\
	meta_list.c\
	meta_convert.c\
	meta_regex.c\
	meta_stringmap.c

libmetad_a_SOURCES=$(libmeta_a_SOURCES)

# Flags.
# -D_BSD_SOURCE is to get the prototype for strerror_r.
# _SVID_SOURCE will do the same thing.

libmeta_a_CFLAGS=\
	-D_GNU_SOURCE -std=gnu2x -DNDEBUG -pthread\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-ffunction-sections -fdata-sections\
	-O3 -Wall -Wno-long-long  -ftree-vectorize\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast\
	-fomit-frame-pointer -msse4.2

libmetad_a_CFLAGS=\
	-D_GNU_SOURCE -std=gnu2x -pthread\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O0 -ggdb -Wall -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

if WITH_VALGRIND
libmeta_a_CFLAGS+=-DWITH_VALGRIND=1 
endif

TESTS=\
	convert_check\
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
	regex_check\
	cache_check\
	filecache_check\
	stringmap_check\
	miscfunc_check\
	pair_check\
	slotbuf_check\
	tcp_server_check

check_PROGRAMS=\
	convert_check\
	slotbuf_check\
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
	regex_check\
	cache_check\
	filecache_check\
	stringmap_check\
	miscfunc_check\
	pair_check\
	tcp_server_check


AM_CFLAGS=-Wall --std=gnu2x -Wextra -Werror -O0 -ggdb
# Broken atm, with false positives
# FANALYZER=-fanalyzer

tcp_server_check_SOURCES=tcp_server.c threadpool.c meta_membuf.c meta_pool.c\
	gensocket.c sslsocket.c tcpsocket.c connection.c\
	meta_process.c cstring.c meta_common.c
tcp_server_check_CFLAGS=-DCHECK_TCP_SERVER $(AM_CFLAGS) $(FANALYZER)

tcp_server_check_LDFLAGS=
tcp_server_check_LDADD=-lssl -lcrypto -lpthread

list_check_SOURCES=meta_list.c
list_check_CFLAGS=-DLIST_CHECK $(AM_CFLAGS) $(FANALYZER)

bitset_check_SOURCES=meta_bitset.c meta_common.c
bitset_check_CFLAGS=-DBITSET_CHECK $(AM_CFLAGS) $(FANALYZER)

configfile_check_SOURCES=meta_configfile.c meta_common.c
configfile_check_CFLAGS=-DCHECK_CONFIGFILE $(AM_CFLAGS)  $(FANALYZER)

cstring_check_SOURCES=cstring.c meta_common.c
cstring_check_CFLAGS=-DCHECK_CSTRING $(AM_CFLAGS) -p -fprofile-arcs -ftest-coverage

membuf_check_SOURCES=meta_membuf.c meta_common.c
membuf_check_CFLAGS=-DCHECK_MEMBUF $(AM_CFLAGS) $(FANALYZER)

pool_check_SOURCES=meta_pool.c meta_array.c meta_common.c
pool_check_CFLAGS=-DCHECK_POOL $(AM_CFLAGS) $(FANALYZER)

array_check_SOURCES=meta_array.c meta_common.c
array_check_CFLAGS=-DCHECK_ARRAY $(AM_CFLAGS) $(FANALYZER)

cache_check_SOURCES=meta_cache.c meta_list.c meta_common.c
cache_check_CFLAGS=-DCHECK_CACHE $(AM_CFLAGS) $(FANALYZER)

filecache_check_SOURCES=meta_filecache.c meta_stringmap.c meta_cache.c meta_list.c meta_common.c
filecache_check_CFLAGS=-DCHECK_FILECACHE $(AM_CFLAGS) $(FANALYZER)

stringmap_check_SOURCES=meta_stringmap.c meta_list.c meta_common.c
stringmap_check_CFLAGS=-DCHECK_STRINGMAP $(AM_CFLAGS) $(FANALYZER)

miscfunc_check_SOURCES=meta_misc.c meta_common.c
miscfunc_check_CFLAGS=-DCHECK_MISCFUNC $(AM_CFLAGS) $(FANALYZER)

pair_check_SOURCES=meta_pair.c meta_common.c
pair_check_CFLAGS=-DCHECK_PAIR $(AM_CFLAGS) $(FANALYZER)

fifo_check_SOURCES=meta_fifo.c meta_wlock.c meta_common.c
fifo_check_CFLAGS=-DCHECK_FIFO $(AM_CFLAGS) $(FANALYZER)

wlock_check_SOURCES=meta_wlock.c meta_common.c
wlock_check_CFLAGS=-DCHECK_WLOCK -O0 -ggdb -D_GNU_SOURCE -std=gnu2x -D_REENTRANT $(AM_CFLAGS) $(FANALYZER)
#wlock_check_CFLAGS=-DCHECK_WLOCK -O0 -ggdb $(AM_CFLAGS)

stack_check_SOURCES=meta_stack.c meta_list.c meta_common.c
stack_check_CFLAGS=-DCHECK_STACK -O0 -ggdb $(AM_CFLAGS) $(FANALYZER)

sampler_check_SOURCES=meta_sampler.c meta_common.c
sampler_check_CFLAGS=-DCHECK_SAMPLER $(AM_CFLAGS) $(FANALYZER)
sampler_check_LDADD=-lpthread
sampler_check_LDFLAGS=-ggdb -O0

slotbuf_check_SOURCES=meta_slotbuf.c  meta_common.c
slotbuf_check_CFLAGS=-DCHECK_SLOTBUF $(AM_CFLAGS) $(FANALYZER)

convert_check_SOURCES=meta_convert.c meta_common.c
convert_check_CFLAGS=-DCHECK_CONVERT $(AM_CFLAGS) $(FANALYZER)

regex_check_SOURCES=meta_regex.c meta_common.c
regex_check_CFLAGS=-DCHECK_REGEX $(AM_CFLAGS)  $(FANALYZER)

LDADD=-lpthread
