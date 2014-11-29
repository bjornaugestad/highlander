# The main target
lib_LTLIBRARIES=libmeta.la libmetad.la

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
	meta_socket.h\
	meta_membuf.h\
	meta_stack.h\
	meta_bitset.h\
	meta_misc.h\
	meta_cache.h\
	meta_configfile.h\
	meta_filecache.h\
	meta_stringmap.h\
	meta_map.h\
	meta_tsb.h\
	connection.h\
	meta_pair.h\
	meta_error.h\
	meta_wlock.h\
	threadpool.h

libmeta_la_LDFLAGS=-release $(PACKAGE_VERSION) -version-info 0:0:0
libmetad_la_LDFLAGS=-release $(PACKAGE_VERSION) -version-info 0:0:0

libmeta_la_SOURCES=\
	meta_common.c\
	meta_sampler.c\
	meta_wlock.c\
	meta_ticker.c\
	meta_slotbuf.c\
	meta_map.c\
	meta_stack.c\
	meta_bitset.c\
	meta_process.c\
	meta_socket.c\
	meta_error.c\
	meta_tsb.c\
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
	meta_stringmap.c

libmetad_la_SOURCES=$(libmeta_la_SOURCES)

# Flags.
# -D_BSD_SOURCE is to get the prototype for strerror_r.
# _SVID_SOURCE will do the same thing.

libmeta_a_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT -pthread\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O2 -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

libmetad_a_CFLAGS=\
	-D_GNU_SOURCE -std=gnu99 -D_REENTRANT -pthread\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-O0 -ggdb -Wall -pedantic -Wno-long-long  -ftree-vectorize -Wno-inline\
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

if WITH_VALGRIND
libmeta_a_CFLAGS+=-DWITH_VALGRIND=1 
endif

