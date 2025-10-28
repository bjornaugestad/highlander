# Wrapper around the actual makefile. This file configures the actual makefile
# via variables and then runs the actual makefile.
# It's a neat solution so that make works from within vim and we don't need any
# wrapper scripts
#
# Notes:
# - gcc does not like -Wnull-reference. Either that or I have a bug in membuf. clang is silent.
# - -Wunreachable-code is nonsense to use as it doesn't get that "if (1)" is intentional.
#
#.PHONY: all gcc clang debug gcc_debug clang_debug san clang_san gcc_san clang_tsan

actual=makefile.actual

# Shared between gcc and clang.
COMMON_CFLAGS=-Wall -Wextra -Wpedantic -Werror -std=gnu2x -Wstrict-prototypes \
	-Wmissing-prototypes -Wwrite-strings -Wshadow -Wcast-align -Wpointer-arith\
	-Wformat-security -Wdouble-promotion -Wuninitialized -Wvla -Wmisleading-indentation\
	-Wconversion -Wsign-conversion -Wbad-function-cast 

COMMON_INCLUDE=-Imeta/src -I http/src

# GCC specific stuff
COMMON_GCC_CFLAGS=$(COMMON_CFLAGS) $(COMMON_INCLUDE) -Warith-conversion
CFLAGS_GCC_DEBUG=$(COMMON_GCC_CFLAGS) -Og -g -D_FORTIFY_SOURCE=2
CFLAGS_GCC_SAN=$(COMMON_GCC_CFLAGS) -Og -g -fsanitize=address,undefined,leak -fno-omit-frame-pointer
CFLAGS_GCC_RELEASE=$(COMMON_GCC_CFLAGS) -O3 -DNDEBUG

# clang specific stuff
COMMON_CLANG_CFLAGS=$(COMMON_CFLAGS) $(COMMON_INCLUDE) -Wnull-dereference
CFLAGS_CLANG_DEBUG=$(COMMON_CLANG_CFLAGS) -Og -g -D_FORTIFY_SOURCE=2
CFLAGS_CLANG_SAN=$(COMMON_CLANG_CFLAGS) -Og -g -fsanitize=address,undefined,leak -fno-omit-frame-pointer
CFLAGS_CLANG_TSAN=$(COMMON_CLANG_CFLAGS) -Og -g -fsanitize=thread -fno-omit-frame-pointer
CFLAGS_CLANG_RELEASE=$(COMMON_CLANG_CFLAGS) -O3 -DNDEBUG

# Coverage with clang/llvm. COMMON_CFLAGS are a bit over the top here, so we use our own.
CFLAGS_CLANG_COV=-Werror -Wall -Wextra -std=gnu2x -O0 -g -fno-inline -fprofile-instr-generate -fcoverage-mapping $(COMMON_INCLUDE)
LDFLAGS_CLANG_COV=-fprofile-instr-generate 

#all:  gcc_san # gcc clang cov
all:   gcc clang cov

gcc: gcc_debug gcc_release gcc_san
clang: clang_debug clang_release clang_san clang_tsan

debug: gcc_debug clang_debug
release: gcc_release clang_release
san: gcc_san clang_san clang_tsan

gcc_debug:
	@make -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_DEBUG)" OUTDIR=.build/gcc/debug all

gcc_san:
	@make -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_SAN)" OUTDIR=.build/gcc/san all 

gcc_release:
	@make -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_RELEASE)" OUTDIR=.build/gcc/release all

clang_debug :
	@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_DEBUG)" OUTDIR=.build/clang/debug all

cov:
	@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_COV)" OUTDIR=.build/clang/cov all

clang_san :
	@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_SAN)" OUTDIR=.build/clang/san all

clang_tsan:
	@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_TSAN)" OUTDIR=.build/clang/tsan all

clang_release :
	@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_RELEASE)" OUTDIR=.build/clang/release all

clean: gcc_clean clang_clean

gcc_clean:
	-@make -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_DEBUG)" OUTDIR=.build/gcc/debug clean
	-@make -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_SAN)" OUTDIR=.build/gcc/san clean
	-@make -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_RELEASE)" OUTDIR=.build/gcc/release clean

clang_clean:
	-@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_DEBUG)" OUTDIR=.build/clang/debug clean
	-@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_SAN)" OUTDIR=.build/clang/san clean
	-@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_TSAN)" OUTDIR=.build/clang/tsan clean
	-@make -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_RELEASE)" OUTDIR=.build/clang/release clean


# Let check depend on all, and then run checks with -j1 to avoid raceconds on port numbers
check: all
	-@make -j1 -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_DEBUG)" OUTDIR=.build/gcc/debug check
	-@make -j1 -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_SAN)" OUTDIR=.build/gcc/san check
	-@make -j1 -f $(actual) CC=gcc CFLAGS="$(CFLAGS_GCC_RELEASE)" OUTDIR=.build/gcc/release check
	-@make -j1 -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_DEBUG)" OUTDIR=.build/clang/debug check
	-@make -j1 -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_SAN)" OUTDIR=.build/clang/san check
	-@make -j1 -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_TSAN)" OUTDIR=.build/clang/tsan check
	-@make -j1 -f $(actual) CC=clang CFLAGS="$(CFLAGS_CLANG_RELEASE)" OUTDIR=.build/clang/release check
