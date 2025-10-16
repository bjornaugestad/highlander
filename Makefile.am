SUBDIRS=tools meta http apps 
LDADD=-lpthread -L.
#AUTOMAKE_OPTIONS=subdir-objects

# Set up environment for sanitizers

AM_TESTS_ENVIRONMENT = \
  LD_PRELOAD= \
  ASAN_OPTIONS='abort_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0' \
  UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1' \
  TSAN_OPTIONS='halt_on_error=1' \
  LSAN_OPTIONS='suppressions=$(srcdir)/lsan.supp' 
 
export AM_TESTS_ENVIRONMENT


EXTRA_DIST=COPYING INSTALL AUTHORS NEWS README TODO
