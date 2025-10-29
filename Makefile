# Wrapper around the actual makefile. This file configures the actual makefile
# via variables and then runs the actual makefile.
# It's a neat solution so that make works from within vim and we don't need any
# wrapper scripts
#
# Notes:
# - gcc does not like -Wnull-reference. Either that or I have a bug in membuf. clang is silent.
# - -Wunreachable-code is nonsense to use as it doesn't get that "if (1)" is intentional.
#

actual=makefile.actual

CC=gcc
# Shared between gcc and clang. (clang's out. Too many targets: 20251029)
COMMON_CFLAGS=-Wall -Wextra -Wpedantic -Werror -std=gnu2x -Wstrict-prototypes \
	-Wmissing-prototypes -Wwrite-strings -Wshadow -Wcast-align -Wpointer-arith\
	-Wformat-security -Wdouble-promotion -Wuninitialized -Wvla -Wmisleading-indentation\
	-Wconversion -Wsign-conversion -Wbad-function-cast 


STATIC_CFLAGS=-fno-pic -fno-PIE -fno-plt -ffunction-sections -fdata-sections
STATIC_LDFLAGS=-static -fno-PIE -Wl,--gc-sections -s

# Self-built openssl from now on
COMMON_INCLUDE=-Imeta/src -I http/src -I $(HOME)/opt/openssl-3.3.5/include
COMMON_LDFLAGS=-L $(HOME)/opt/openssl-3.3.5/lib 

# GCC specific stuff
COMMON_GCC_CFLAGS=$(COMMON_CFLAGS) $(COMMON_INCLUDE) -Warith-conversion
CFLAGS_GCC_DEBUG=$(COMMON_GCC_CFLAGS) -Og -g -D_FORTIFY_SOURCE=2
CFLAGS_GCC_SAN=$(COMMON_GCC_CFLAGS) -Og -g -fsanitize=address,undefined,leak -fno-omit-frame-pointer
CFLAGS_GCC_RELEASE=$(COMMON_GCC_CFLAGS) -O3 -DNDEBUG

all:  gcc 

gcc: gcc_debug gcc_release gcc_san

debug: gcc_debug 
release: gcc_release 
san: gcc_san 

gcc_debug:
	@make -f $(actual) CFLAGS="$(CFLAGS_GCC_DEBUG)" LDFLAGS="$(COMMON_LDFLAGS) $(STATIC_LDFLAGS)"  OUTDIR=.build/gcc/debug all

gcc_san:
	@make -f $(actual) CFLAGS="$(CFLAGS_GCC_SAN)" LDFLAGS="$(COMMON_LDFLAGS)"  OUTDIR=.build/gcc/san all 

gcc_release:
	@make -f $(actual) CFLAGS="$(CFLAGS_GCC_RELEASE) $(STATIC_CFLAGS)" LDFLAGS="$(COMMON_LDFLAGS) $(STATIC_LDFLAGS)"  OUTDIR=.build/gcc/release all


clean: gcc_clean 

gcc_clean:
	-@make -f $(actual) CC=$(CC) CFLAGS="$(CFLAGS_GCC_DEBUG)" LDFLAGS="$(COMMON_LDFLAGS)"  OUTDIR=.build/gcc/debug clean
	-@make -f $(actual) CC=$(CC) CFLAGS="$(CFLAGS_GCC_SAN)" LDFLAGS="$(COMMON_LDFLAGS)"  OUTDIR=.build/gcc/san clean
	-@make -f $(actual) CC=$(CC) CFLAGS="$(CFLAGS_GCC_RELEASE)" LDFLAGS="$(COMMON_LDFLAGS)"  OUTDIR=.build/gcc/release clean

# Let check depend on all, and then run checks with -j1 to avoid raceconds on port numbers
check: all
	-@make -j1 -f $(actual) CC=$(CC) CFLAGS="$(CFLAGS_GCC_DEBUG)" LDFLAGS="$(COMMON_LDFLAGS)"  OUTDIR=.build/gcc/debug check
	-@make -j1 -f $(actual) CC=$(CC) CFLAGS="$(CFLAGS_GCC_SAN)" LDFLAGS="$(COMMON_LDFLAGS)"  OUTDIR=.build/gcc/san check
	-@make -j1 -f $(actual) CC=$(CC) CFLAGS="$(CFLAGS_GCC_RELEASE)" LDFLAGS="$(COMMON_LDFLAGS)"  OUTDIR=.build/gcc/release check
