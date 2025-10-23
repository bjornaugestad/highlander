#!/bin/bash
# build script for highlander. Runs make with the proper flags.
# Passes whatever args to make

COMMON_CFLAGS="-Wall -Wextra -Wpedantic -Werror -std=gnu2x -Wstrict-prototypes \
	-Wmissing-prototypes -Wwrite-strings -Wshadow -Wcast-align -Wpointer-arith\
	-Wformat-security -Wdouble-promotion -Wuninitialized"

COMMON_INCLUDE="-Imeta/src -I http/src"

COMMON_GCC_CFLAGS="$COMMON_CFLAGS $COMMON_INCLUDE"
CFLAGS_GCC_DEBUG="$COMMON_GCC_CFLAGS -O0 -g"
CFLAGS_GCC_SAN="$COMMON_GCC_CFLAGS -O0 -g -fsanitize=address,undefined,leak -fno-omit-frame-pointer"
CFLAGS_GCC_RELEASE="$COMMON_GCC_CFLAGS -O3 -DNDEBUG"

COMMON_CLANG_CFLAGS="$COMMON_CFLAGS $COMMON_INCLUDE"
CFLAGS_CLANG_DEBUG="$COMMON_CLANG_CFLAGS -O0 -g"
CFLAGS_CLANG_SAN="$COMMON_CLANG_CFLAGS -O0 -g -fsanitize=address,undefined,leak -fno-omit-frame-pointer"
CFLAGS_CLANG_TSAN="$COMMON_CLANG_CFLAGS -O0 -g -fsanitize=thread -fno-omit-frame-pointer"
CFLAGS_CLANG_RELEASE="$COMMON_CLANG_CFLAGS -O3 -DNDEBUG"

make CC=gcc CFLAGS="$CFLAGS_GCC_DEBUG" OUTDIR=.build/gcc/debug $*
make CC=gcc CFLAGS="$CFLAGS_GCC_SAN" OUTDIR=.build/gcc/san $*
make CC=gcc CFLAGS="$CFLAGS_GCC_RELEASE" OUTDIR=.build/gcc/release $*

make CC=clang CFLAGS="$CFLAGS_CLANG_DEBUG" OUTDIR=.build/clang/debug $*
make CC=clang CFLAGS="$CFLAGS_CLANG_SAN" OUTDIR=.build/clang/san $*
make CC=clang CFLAGS="$CFLAGS_CLANG_TSAN" OUTDIR=.build/clang/tsan $*
make CC=clang CFLAGS="$CFLAGS_CLANG_RELEASE" OUTDIR=.build/clang/release $*
