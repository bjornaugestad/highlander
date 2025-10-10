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




AM_CFLAGS=\
	-std=gnu2x -D_REENTRANT\
	-I$(srcdir)\
	-I$(top_srcdir)/src\
	-Werror -Wshadow -W -Wmissing-prototypes\
	-Wformat-overflow=2 -Woverflow -Wshift-overflow=2 -Wstrict-overflow=5\
	-O2 -ggdb -Wextra -Wall -pedantic -Wno-long-long  -Wno-inline

EXTRA_DIST=COPYING INSTALL AUTHORS NEWS README TODO
