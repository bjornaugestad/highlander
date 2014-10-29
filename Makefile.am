SUBDIRS=tools src webcache ftp
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
	-Wshadow -W -Wmissing-prototypes\
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

EXTRA_DIST=$(man_MANS)\
	COPYING  INSTALL AUTHORS NEWS README TODO


man_MANS=\
	doc/attribute_free.3\
	doc/attribute_get_charset.3\
	doc/attribute_get_encoding.3\
	doc/attribute_get_language.3\
	doc/attribute_get_media_type.3\
	doc/attribute_new.3\
	doc/attribute_set_authorization.3\
	doc/attribute_set_charset.3\
	doc/attribute_set_encoding.3\
	doc/attribute_set_language.3\
	doc/attribute_set_media_type.3\
	doc/cookie_get_comment.3\
	doc/cookie_get_domain.3\
	doc/cookie_get_max_age.3\
	doc/cookie_get_name.3\
	doc/cookie_get_path.3\
	doc/cookie_get_secure.3\
	doc/cookie_get_value.3\
	doc/cookie_get_version.3\
	doc/cookie_new.3\
	doc/cookie_set_comment.3\
	doc/cookie_set_domain.3\
	doc/cookie_set_max_age.3\
	doc/cookie_set_name.3\
	doc/cookie_set_path.3\
	doc/cookie_set_secure.3\
	doc/cookie_set_value.3\
	doc/cookie_set_version.3\
	doc/get_basename.3\
	doc/get_mime_type.3\
	doc/highlander_attributes.3\
	doc/highlander_cookies.3\
	doc/highlander.3\
	doc/http_server_add_page.3\
	doc/http_server_alloc.3\
	doc/http_server_configure.3\
	doc/http_server_free_root_resources.3\
	doc/http_server_get_block_when_full.3\
	doc/http_server_get_document_root.3\
	doc/http_server_get_queue_size.3\
	doc/http_server_get_root_resources.3\
	doc/http_server_get_timeout_accept.3\
	doc/http_server_get_timeout_read.3\
	doc/http_server_get_timeout_write.3\
	doc/http_server_new.3\
	doc/http_server_set_block_when_full.3\
	doc/http_server_set_can_read_files.3\
	doc/http_server_set_default_page_attributes.3\
	doc/http_server_set_default_page_handler.3\
	doc/http_server_set_documentroot.3\
	doc/http_server_set_host.3\
	doc/http_server_set_logfile.3\
	doc/http_server_set_logrotate.3\
	doc/http_server_set_max_pages.3\
	doc/http_server_set_port.3\
	doc/http_server_set_post_limit.3\
	doc/http_server_set_queue_size.3\
	doc/http_server_set_retries_read.3\
	doc/http_server_set_retries_write.3\
	doc/http_server_set_timeout_accept.3\
	doc/http_server_set_timeout_read.3\
	doc/http_server_set_timeout_write.3\
	doc/http_server_set_worker_threads.3\
	doc/http_server_shutdown.3\
	doc/http_server_shutting_down.3\
	doc/http_server_start.3\
	doc/http_server_start_via_process.3\
	doc/request_accepts_language.3\
	doc/request_accepts_media_type.3\
	doc/request_content_type_is.3\
	doc/request_get_content_length.3\
	doc/request_get_content.3\
	doc/request_get_content_type.3\
	doc/request_get_cookie_count.3\
	doc/request_get_cookie.3\
	doc/request_get_field_count.3\
	doc/request_get_host.3\
	doc/request_get_method.3\
	doc/request_get_parameter_count.3\
	doc/request_get_parameter_name.3\
	doc/request_get_parameter_value.3\
	doc/request_get_referer.3\
	doc/request_get_uri.3\
	doc/request_get_user_agent.3\
	doc/request_get_version.3\
	doc/response_add.3\
	doc/response_printf.3\
	doc/response_send_file.3\
	doc/response_set_accept_ranges.3\
	doc/response_set_age.3\
	doc/response_set_allow.3\
	doc/response_set_cache_control.3\
	doc/response_set_connection.3\
	doc/response_set_content_buffer.3\
	doc/response_set_content_encoding.3\
	doc/response_set_content_language.3\
	doc/response_set_cookie.3\
	doc/response_set_date.3\
	doc/response_set_etag.3\
	doc/response_set_expires.3\
	doc/response_set_last_modified.3\
	doc/response_set_location.3\
	doc/response_set_pragma.3\
	doc/response_set_proxy_authenticate.3\
	doc/response_set_retry_after.3\
	doc/response_set_server.3\
	doc/response_set_trailer.3\
	doc/response_set_transfer_encoding.3\
	doc/response_set_upgrade.3\
	doc/response_set_vary.3\
	doc/response_set_via.3\
	doc/response_set_warning.3\
	doc/response_set_www_authenticate.3\
	doc/td2p.3\
	doc/hipp.1\
	\
	doc/cstring.3\
	doc/cstring_charcat.3\
	doc/cstring_concat.3\
	doc/cstring_copy.3\
	doc/cstring_extend.3\
	doc/cstring_left.3\
	doc/cstring_length.3\
	doc/cstring_lower.3\
	doc/cstring_new.3\
	doc/cstring_pcat.3\
	doc/cstring_printf.3\
	doc/cstring_recycle.3\
	doc/cstring_reverse.3\
	doc/cstring_right.3\
	doc/cstring_strip.3\
	doc/cstring_substring.3\
	doc/cstring_upper.3\
	doc/cstring_vprintf.3\
	\
	doc/list_add.3\
	doc/list_copy.3\
	doc/list_count.3\
	doc/list_delete.3\
	doc/list_end.3\
	doc/list_find.3\
	doc/list_first.3\
	doc/list_foreach.3\
	doc/list_get.3\
	doc/list_get_item.3\
	doc/list_insert.3\
	doc/list_last.3\
	doc/list_merge.3\
	doc/list_new.3\
	doc/list_next.3\
	doc/list_remove_node.3\
	doc/list_size.3\
	doc/list_sort.3\
	doc/sublist_adaptor.3\
	doc/sublist_copy.3\
	doc/sublist_create.3\
	doc/sublist_free.3\
	\
	doc/meta_process.3\
	doc/process_add_object_to_start.3\
	doc/process_free.3\
	doc/process_get_exitcode.3\
	doc/process_new.3\
	doc/process_set_rootdir.3\
	doc/process_set_username.3\
	doc/process_shutting_down.3\
	doc/process_start.3\
	doc/process_wait_for_shutdown.3\
	\
	doc/meta_pool.3\
	doc/pool_new.3\
	doc/pool_free.3\
	doc/pool_add.3\
	doc/pool_get.3\
	doc/pool_recycle.3\
	\
	doc/meta_array.3\
	doc/array_new.3\
	doc/array_free.3\
	doc/array_nelem.3\
	doc/array_extend.3\
	doc/array_get.3\
	doc/array_add.3\
	\
	doc/meta_bitset.3\
	doc/bitset_new.3\
	doc/bitset_free.3\
	doc/bitset_clear.3\
	doc/bitset_clear_all.3\
	doc/bitset_data.3\
	doc/bitset_is_set.3\
	doc/bitset_map.3\
	doc/bitset_remap.3\
	doc/bitset_set.3\
	doc/bitset_set_all.3\
	doc/bitset_size.3\
	doc/bitset_unmap.3\
	\
	doc/meta_cache.3\
	doc/cache_add.3\
	doc/cache_exists.3\
	doc/cache_free.3\
	doc/cache_get.3\
	doc/cache_new.3\
	doc/cache_remove.3\
	\
	doc/meta_configfile.3\
	doc/configfile_exists.3\
	doc/configfile_free.3\
	doc/configfile_get_int.3\
	doc/configfile_get_long.3\
	doc/configfile_get_string.3\
	doc/configfile_get_uint.3\
	doc/configfile_get_ulong.3\
	doc/configfile_read.3\
	\
 #


