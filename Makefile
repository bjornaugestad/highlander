# Just a simple Makefile without those nightmareish build systems(Automake, Meson)
# boa@20251023
#

# Some default settings

# What do we build? libmeta.a libhighlander.a test programs and app-demos.
# Where does the output go? We want output directories per toolchain and build type.
# Which toolchains do we support? clang and gcc.
# Which build types? release, debug, tsan, ubsan+asan
# Since gcc cannot build tsan, we end up with seven invariants:
# 	- seven CFLAGS, OUTPUTDIR, and so forth
# 		- gcc_release, gcc_debug, gcc_san
# 		- glang_release, clang_debug, clang_tsan, clang_san
#   So we build seven invariants and place the output in seven directories under .build
#   .build/gcc/{release,debug,san}
#   .build/clang/{release,debug,tsan,san}
#

# Naming of libraries: We have two libs, meta and highlander. They're built in
# seven ways. Do we want seven names? What do we install? Let's install
# libmeta.a and libhighlander.a to $(PREFIX) which defaults to $(HOME),
# so put headers in $(PREFIX)/include and so forth.

LIBMETA_SOURCES=\
	meta/src/meta_convert.c meta/src/meta_bitset.c meta/src/meta_process.c\
	meta/src/meta_pool.c meta/src/meta_common.c meta/src/cstring.c\
	meta/src/meta_error.c meta/src/meta_stringmap.c meta/src/gensocket.c\
	meta/src/meta_wlock.c meta/src/meta_map.c meta/src/meta_list.c\
	meta/src/connection.c meta/src/meta_filecache.c meta/src/meta_ticker.c\
	meta/src/meta_stack.c meta/src/meta_slotbuf.c meta/src/miscssl.c\
	meta/src/meta_misc.c meta/src/meta_regex.c meta/src/meta_sampler.c\
	meta/src/meta_pair.c meta/src/meta_membuf.c meta/src/meta_fifo.c\
	meta/src/meta_configfile.c meta/src/tcp_client.c meta/src/meta_cache.c\
	meta/src/meta_array.c meta/src/threadpool.c

LIBHIGHLANDER_SOURCES=\
	http/src/cookies.c http/src/parse_time.c http/src/rfc1738.c\
	http/src/send_status_code.c http/src/highlander.c\
	http/src/html_template_repository.c http/src/entity_header.c\
	http/src/html_section.c http/src/parse_http.c http/src/http_client.c\
	http/src/attribute.c http/src/request.c http/src/dynamic_page.c\
	http/src/html_buffer.c http/src/general_header.c http/src/html_menu.c\
	http/src/http.c http/src/response.c http/src/html_template.c\
	http/src/http_server.c meta/src/tcp_server.c

# Test programs for meta. We build them from source and place them in OUTDIR.
# We need rules per program due to the -DCHECK_foo argument, but we can
# compile directly from multiple source files to one executable.
META_TESTS=$(OUTDIR)/array_check $(OUTDIR)/bitset_check $(OUTDIR)/cache_check \
	$(OUTDIR)/configfile_check $(OUTDIR)/convert_check $(OUTDIR)/cstring_check \
	$(OUTDIR)/fifo_check $(OUTDIR)/filecache_check $(OUTDIR)/list_check \
	$(OUTDIR)/membuf_check $(OUTDIR)/miscfunc_check $(OUTDIR)/pair_check \
	$(OUTDIR)/pool_check $(OUTDIR)/regex_check $(OUTDIR)/sampler_check \
	$(OUTDIR)/slotbuf_check $(OUTDIR)/stack_check $(OUTDIR)/stringmap_check \
	$(OUTDIR)/tcp_client_check $(OUTDIR)/tcp_server_check $(OUTDIR)/wlock_check 

$(OUTDIR)/array_check: meta/src/meta_array.c
	$(CC) $(CFLAGS) -DCHECK_ARRAY -o $@ $^
	$@ # run it too

$(OUTDIR)/bitset_check : meta/src/meta_bitset.c
	$(CC) $(CFLAGS) -DCHECK_BITSET -o $@ $^
	$@ # run it too

$(OUTDIR)/cache_check : meta/src/meta_cache.c meta/src/meta_list.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_CACHE -o $@ $^
	$@ # run it too

$(OUTDIR)/configfile_check : meta/src/meta_configfile.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_CONFIGFILE -o $@ $^
	# broken $@ # run it too

$(OUTDIR)/convert_check : meta/src/meta_convert.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_CONVERT -o $@ $^
	$@ # run it too

$(OUTDIR)/cstring_check : meta/src/cstring.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_CSTRING -o $@ $^
	$@ # run it too

$(OUTDIR)/fifo_check : meta/src/meta_fifo.c meta/src/meta_wlock.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_FIFO -o $@ $^
	$@ # run it too

$(OUTDIR)/filecache_check : meta/src/meta_filecache.c meta/src/meta_stringmap.c meta/src/meta_cache.c meta/src/meta_list.c  meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_FILECACHE -o $@ $^
	$@ # run it too

$(OUTDIR)/list_check : meta/src/meta_list.c
	$(CC) $(CFLAGS) -DCHECK_LIST -o $@ $^
	$@ # run it too

$(OUTDIR)/membuf_check : meta/src/meta_membuf.c
	$(CC) $(CFLAGS) -DCHECK_MEMBUF -o $@ $^
	$@ # run it too

$(OUTDIR)/miscfunc_check : meta/src/meta_misc.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_MISCFUNC -o $@ $^
	$@ # run it too

$(OUTDIR)/pair_check : meta/src/meta_pair.c
	$(CC) $(CFLAGS) -DCHECK_PAIR -o $@ $^
	$@ # run it too

$(OUTDIR)/pool_check : meta/src/meta_pool.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_POOL -o $@ $^
	$@ # run it too

$(OUTDIR)/regex_check : meta/src/meta_regex.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_REGEX -o $@ $^
	$@ # run it too

$(OUTDIR)/sampler_check : meta/src/meta_sampler.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_SAMPLER -o $@ $^
	$@ # run it too

$(OUTDIR)/slotbuf_check : meta/src/meta_slotbuf.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_SLOTBUF -o $@ $^
	$@ # run it too

$(OUTDIR)/stack_check : meta/src/meta_stack.c meta/src/meta_list.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_STACK -o $@ $^
	$@ # run it too

$(OUTDIR)/stringmap_check : meta/src/meta_stringmap.c meta/src/meta_list.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_STRINGMAP -o $@ $^
	$@ # run it too

$(OUTDIR)/tcp_client_check : meta/src/tcp_client.c meta/src/threadpool.c meta/src/meta_membuf.c meta/src/gensocket.c meta/src/connection.c meta/src/cstring.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_TCP_CLIENT -o $@ $^ -lssl -lcrypto
	$@ # run it too

$(OUTDIR)/tcp_server_check : meta/src/tcp_server.c meta/src/threadpool.c meta/src/meta_membuf.c meta/src/meta_pool.c meta/src/gensocket.c meta/src/connection.c meta/src/meta_process.c meta/src/cstring.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_TCP_SERVER -o $@ $^ -lssl -lcrypto
	$@ # run it too

$(OUTDIR)/wlock_check : meta/src/meta_wlock.c meta/src/meta_common.c
	$(CC) $(CFLAGS) -DCHECK_WLOCK -o $@ $^
	$@ # run it too

# Convert list of source files to list of object files
LIBMETA_O=$(patsubst %.c, $(OUTDIR)/%.o, $(LIBMETA_SOURCES))
LIBHIGHLANDER_O=$(patsubst %.c, $(OUTDIR)/%.o, $(LIBHIGHLANDER_SOURCES))

# Create output directory if needed
$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(OUTDIR)/%.o: %.c | $(OUTDIR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Start with building libmeta.a
TARGETS=$(OUTDIR)/libmeta.a $(OUTDIR)/libhighlander.a $(META_TESTS)
all: $(TARGETS)

$(OUTDIR)/libmeta.a: $(LIBMETA_O)
	$(AR) rcs $@ $^

$(OUTDIR)/libhighlander.a: $(LIBHIGHLANDER_O)
	$(AR) rcs $@ $^

clean:
	rm $(LIBMETA_O) $(LIBHIGHLANDER_O) $(TARGETS)
