#AUTOMAKE_OPTIONS=subdir-objects

# The main target
lib_LIBRARIES=libmeta.a

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
	tcp_client.h\
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
	tcp_client.c\
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

# Flags.
# -D_BSD_SOURCE is to get the prototype for strerror_r.
# _SVID_SOURCE will do the same thing.

libmeta_a_CFLAGS=-I$(srcdir) -I$(top_srcdir)/src

TESTS=\
	meta_process_check\
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
	tcp_server_check\
	tcp_client_check

check_PROGRAMS=\
	meta_process_check\
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
	tcp_server_check\
	tcp_client_check


meta_process_check_SOURCES=\
	meta_process.c cstring.c meta_common.c
meta_process_check_CFLAGS=-DCHECK_META_PROCESS 

tcp_server_check_SOURCES=tcp_server.c threadpool.c meta_membuf.c meta_pool.c\
	gensocket.c sslsocket.c tcpsocket.c connection.c\
	meta_process.c cstring.c meta_common.c
tcp_server_check_CFLAGS=-DCHECK_TCP_SERVER 

tcp_server_check_LDFLAGS=
tcp_server_check_LDADD=-lssl -lcrypto -lpthread

tcp_client_check_SOURCES=tcp_client.c threadpool.c meta_membuf.c \
	gensocket.c sslsocket.c tcpsocket.c connection.c cstring.c meta_common.c
tcp_client_check_CFLAGS=-DCHECK_TCP_CLIENT 

tcp_client_check_LDFLAGS=
tcp_client_check_LDADD=-lssl -lcrypto -lpthread

list_check_SOURCES=meta_list.c
list_check_CFLAGS=-DLIST_CHECK 

bitset_check_SOURCES=meta_bitset.c meta_common.c
bitset_check_CFLAGS=-DBITSET_CHECK 

configfile_check_SOURCES=meta_configfile.c meta_common.c
configfile_check_CFLAGS=-DCHECK_CONFIGFILE  

cstring_check_SOURCES=cstring.c meta_common.c
cstring_check_CFLAGS=-DCHECK_CSTRING  

membuf_check_SOURCES=meta_membuf.c meta_common.c
membuf_check_CFLAGS=-DCHECK_MEMBUF 

pool_check_SOURCES=meta_pool.c meta_array.c meta_common.c
pool_check_CFLAGS=-DCHECK_POOL 

array_check_SOURCES=meta_array.c meta_common.c
array_check_CFLAGS=-DCHECK_ARRAY 

cache_check_SOURCES=meta_cache.c meta_list.c meta_common.c
cache_check_CFLAGS=-DCHECK_CACHE 

filecache_check_SOURCES=meta_filecache.c meta_stringmap.c meta_cache.c meta_list.c meta_common.c
filecache_check_CFLAGS=-DCHECK_FILECACHE 

stringmap_check_SOURCES=meta_stringmap.c meta_list.c meta_common.c
stringmap_check_CFLAGS=-DCHECK_STRINGMAP 

miscfunc_check_SOURCES=meta_misc.c meta_common.c
miscfunc_check_CFLAGS=-DCHECK_MISCFUNC 

pair_check_SOURCES=meta_pair.c meta_common.c
pair_check_CFLAGS=-DCHECK_PAIR 

fifo_check_SOURCES=meta_fifo.c meta_wlock.c meta_common.c
fifo_check_CFLAGS=-DCHECK_FIFO 

wlock_check_SOURCES=meta_wlock.c meta_common.c
wlock_check_CFLAGS=-DCHECK_WLOCK

stack_check_SOURCES=meta_stack.c meta_list.c meta_common.c
stack_check_CFLAGS=-DCHECK_STACK 

sampler_check_SOURCES=meta_sampler.c meta_common.c
sampler_check_CFLAGS=-DCHECK_SAMPLER 
sampler_check_LDADD=-lpthread

slotbuf_check_SOURCES=meta_slotbuf.c  meta_common.c
slotbuf_check_CFLAGS=-DCHECK_SLOTBUF 

convert_check_SOURCES=meta_convert.c meta_common.c
convert_check_CFLAGS=-DCHECK_CONVERT 

regex_check_SOURCES=meta_regex.c meta_common.c
regex_check_CFLAGS=-DCHECK_REGEX  

LDADD=-lpthread
