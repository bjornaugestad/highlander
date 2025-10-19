# Installed headers
install_headers(
  'cstring.h', 
  'meta_atomic.h', 
  'meta_sampler.h', 
  'meta_common.h', 
  'meta_ticker.h', 
  'meta_process.h', 
  'meta_slotbuf.h', 
  'meta_pool.h', 
  'meta_array.h', 
  'meta_list.h', 
  'meta_fifo.h', 
  'miscssl.h', 
  'gensocket.h', 
  'meta_membuf.h', 
  'meta_stack.h', 
  'meta_bitset.h', 
  'meta_misc.h', 
  'meta_cache.h', 
  'meta_configfile.h', 
  'meta_filecache.h', 
  'meta_regex.h', 
  'meta_stringmap.h', 
  'meta_map.h', 
  'connection.h', 
  'meta_pair.h', 
  'meta_error.h', 
  'meta_wlock.h', 
  'meta_convert.h', 
  'tcp_server.h', 
  'tcp_client.h', 
  'threadpool.h')

meta_inc = include_directories('.')
meta_lib = static_library('libmeta', [
	'meta_common.c',
	'meta_sampler.c',
	'meta_wlock.c',
	'meta_ticker.c',
	'meta_slotbuf.c',
	'meta_map.c',
	'meta_stack.c',
	'meta_bitset.c',
	'meta_process.c',
	'miscssl.c',
	'gensocket.c',
	'meta_error.c',
	'tcp_server.c',
	'tcp_client.c',
	'connection.c',
	'cstring.c',
	'meta_membuf.c',
	'meta_fifo.c',
	'meta_pair.c',
	'meta_array.c',
	'meta_misc.c',
	'meta_pool.c',
	'threadpool.c',
	'meta_configfile.c',
	'meta_cache.c',
	'meta_filecache.c',
	'meta_list.c',
	'meta_convert.c',
	'meta_regex.c',
	'meta_stringmap.c'
  ],
  install: true,
  include_directories: meta_inc,
  c_args: global_c_args
)

meta_dep = declare_dependency(
  link_with: meta_lib,
  include_directories: meta_inc,
)

# Auto-generated Meson snippet for tests in meta/src
test_deps = []  # add common deps here if needed (e.g., cmocka)
test_common_c_args = ['-Wall', '--std=gnu2x', '-Wextra', '-Werror', '-O0', '-ggdb', '-Wformat-overflow=2', '-Woverflow', '-Wshift-overflow=2', '-Wstrict-overflow=2']
test_common_link_args = []

convert_check_exe = executable('convert_check', ['meta_convert.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_CONVERT'],
  link_args: test_common_link_args + [],
)
test('convert_check', convert_check_exe)

slotbuf_check_exe = executable('slotbuf_check', ['meta_slotbuf.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_SLOTBUF'],
  link_args: test_common_link_args + [],
)
test('slotbuf_check', slotbuf_check_exe)

sampler_check_exe = executable('sampler_check', ['meta_sampler.c', 'meta_common.c'],
  c_args   : test_common_c_args    + ['-DCHECK_SAMPLER=1'],
  link_args: test_common_link_args + ['-lpthread'],
)
test('sampler_check', sampler_check_exe)

list_check_exe = executable('list_check', ['meta_list.c'],
  c_args: test_common_c_args + ['-DLIST_CHECK=1'],
  link_args: test_common_link_args + [],
)
test('list_check', list_check_exe)

bitset_check_exe = executable('bitset_check', ['meta_bitset.c'],
  c_args: test_common_c_args + ['-DBITSET_CHECK'],
  link_args: test_common_link_args + [],
)
test('bitset_check', bitset_check_exe)

fifo_check_exe = executable('fifo_check', ['meta_fifo.c', 'meta_wlock.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_FIFO'],
  link_args: test_common_link_args + [],
)
test('fifo_check', fifo_check_exe)

wlock_check_exe = executable('wlock_check', ['meta_wlock.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_WLOCK'],
  link_args: test_common_link_args + [],
)
test('wlock_check', wlock_check_exe)

stack_check_exe = executable('stack_check', ['meta_stack.c', 'meta_list.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_STACK'],
  link_args: test_common_link_args + [],
)
test('stack_check', stack_check_exe)

configfile_check_exe = executable('configfile_check', ['meta_configfile.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_CONFIGFILE'],
  link_args: test_common_link_args + [],
)
test('configfile_check', configfile_check_exe)

cstring_check_exe = executable('cstring_check', ['cstring.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_CSTRING'],
  link_args: test_common_link_args + [],
)
test('cstring_check', cstring_check_exe)

membuf_check_exe = executable('membuf_check', ['meta_membuf.c'],
  c_args: test_common_c_args + ['-DCHECK_MEMBUF'],
  link_args: test_common_link_args + [],
)
test('membuf_check', membuf_check_exe)

pool_check_exe = executable('pool_check', ['meta_pool.c', 'meta_array.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_POOL=1'],
  link_args: test_common_link_args + [],
)
test('pool_check', pool_check_exe)

array_check_exe = executable('array_check', ['meta_array.c'],
  c_args: test_common_c_args + ['-DCHECK_ARRAY'],
  link_args: test_common_link_args + [],
)
test('array_check', array_check_exe)

regex_check_exe = executable('regex_check', ['meta_regex.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_REGEX'],
  link_args: test_common_link_args + [],
)
test('regex_check', regex_check_exe)

cache_check_exe = executable('cache_check', ['meta_cache.c', 'meta_list.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_CACHE'],
  link_args: test_common_link_args + [],
)
test('cache_check', cache_check_exe)

filecache_check_exe = executable('filecache_check', [
    'meta_filecache.c',
    'meta_cache.c',
    'meta_stringmap.c',
    'meta_list.c',
    'meta_common.c'
  ],
  c_args: test_common_c_args + ['-DCHECK_FILECACHE'],
  link_args: test_common_link_args + [],
)
test('filecache_check', filecache_check_exe)

stringmap_check_exe = executable('stringmap_check', ['meta_stringmap.c', 'meta_list.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_STRINGMAP=1'],
  link_args: test_common_link_args + [],
)
test('stringmap_check', stringmap_check_exe)

miscfunc_check_exe = executable('miscfunc_check', ['meta_misc.c', 'meta_common.c'],
  c_args: test_common_c_args + ['-DCHECK_MISCFUNC'],
  link_args: test_common_link_args + [],
)
test('miscfunc_check', miscfunc_check_exe)

pair_check_exe = executable('pair_check', ['meta_pair.c'],
  c_args: test_common_c_args + ['-DCHECK_PAIR'],
  link_args: test_common_link_args + [],
)
test('pair_check', pair_check_exe)

tcp_server_check_exe = executable('tcp_server_check', [
    'tcp_server.c',
    'threadpool.c',
    'meta_membuf.c',
    'meta_pool.c',
    'gensocket.c',
    'connection.c',
    'meta_process.c',
    'cstring.c',
    'meta_common.c'
  ],

  c_args: test_common_c_args + ['-DCHECK_TCP_SERVER=1'],
  link_args: test_common_link_args + ['-lssl', '-lcrypto', '-lpthread'],
)
test('tcp_server_check', tcp_server_check_exe)

tcp_client_check_exe = executable('tcp_client_check', [
  'tcp_client.c',
  'threadpool.c',
  'meta_membuf.c',
  'gensocket.c',
  'connection.c',
  'cstring.c',
  'meta_common.c'
],

  c_args: test_common_c_args + ['-DCHECK_TCP_CLIENT'],
  link_args: test_common_link_args + ['-lssl', '-lcrypto', '-lpthread'],
)
test('tcp_client_check', tcp_client_check_exe)
