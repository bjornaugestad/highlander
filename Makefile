# Flat Meson/Ninja wrapper

.PHONY: all test clean reallyclean \
        build_gcc_release build_gcc_debug build_gcc_tsan build_gcc_asan_ubsan \
        build_clang_release build_clang_debug build_clang_tsan build_clang_asan_ubsan \
        test_gcc_release test_gcc_debug test_gcc_tsan test_gcc_asan_ubsan \
        test_clang_release test_clang_debug test_clang_tsan test_clang_asan_ubsan

all: \
  build_gcc_release build_gcc_debug build_gcc_tsan build_gcc_asan_ubsan \
  build_clang_release build_clang_debug build_clang_tsan build_clang_asan_ubsan

test: \
  test_gcc_release test_gcc_debug test_gcc_tsan test_gcc_asan_ubsan \
  test_clang_release test_clang_debug test_clang_tsan test_clang_asan_ubsan

# ===== GCC =====
build_gcc_release:
	CC=gcc meson setup build_gcc_release --reconfigure --buildtype=release -Db_sanitize=none -Dwarning_level=3
	meson compile -C build_gcc_release

build_gcc_debug:
	CC=gcc meson setup build_gcc_debug --reconfigure --buildtype=debug -Db_sanitize=none -Dwarning_level=3
	meson compile -C build_gcc_debug

# note: tsan and gcc may be meh
build_gcc_tsan:
	CC=gcc meson setup build_gcc_tsan --reconfigure --buildtype=debug -Db_sanitize=thread -Dwarning_level=3
	meson compile -C build_gcc_tsan

build_gcc_asan_ubsan:
	CC=gcc meson setup build_gcc_asan_ubsan --reconfigure --buildtype=debug -Db_sanitize=address,undefined -Dwarning_level=3
	meson compile -C build_gcc_asan_ubsan

test_gcc_release: build_gcc_release
	meson test -C build_gcc_release --no-rebuild --print-errorlogs

test_gcc_debug: build_gcc_debug
	meson test -C build_gcc_debug --no-rebuild --print-errorlogs

test_gcc_tsan: build_gcc_tsan
	meson test -C build_gcc_tsan --no-rebuild --print-errorlogs

test_gcc_asan_ubsan: build_gcc_asan_ubsan
	meson test -C build_gcc_asan_ubsan --no-rebuild --print-errorlogs

# ===== Clang =====
build_clang_release:
	CC=clang meson setup build_clang_release --reconfigure --buildtype=release -Db_sanitize=none -Dwarning_level=3
	meson compile -C build_clang_release

build_clang_debug:
	CC=clang meson setup build_clang_debug --reconfigure --buildtype=debug -Db_sanitize=none -Dwarning_level=3
	meson compile -C build_clang_debug

build_clang_tsan:
	CC=clang meson setup build_clang_tsan --reconfigure --buildtype=debug -Db_sanitize=thread -Dwarning_level=3
	meson compile -C build_clang_tsan

build_clang_asan_ubsan:
	CC=clang meson setup build_clang_asan_ubsan --reconfigure --buildtype=debug -Db_sanitize=address,undefined -Dwarning_level=3
	meson compile -C build_clang_asan_ubsan

test_clang_release: build_clang_release
	meson test -C build_clang_release --no-rebuild --print-errorlogs

test_clang_debug: build_clang_debug
	meson test -C build_clang_debug --no-rebuild --print-errorlogs

test_clang_tsan: build_clang_tsan
	meson test -C build_clang_tsan --no-rebuild --print-errorlogs

test_clang_asan_ubsan: build_clang_asan_ubsan
	meson test -C build_clang_asan_ubsan --no-rebuild --print-errorlogs

# ===== Hygiene =====
clean:
	-@[ -d build_gcc_release ] && ninja -C build_gcc_release -q clean || true
	-@[ -d build_gcc_debug ] && ninja -C build_gcc_debug -q clean || true
	-@[ -d build_gcc_tsan ] && ninja -C build_gcc_tsan -q clean || true
	-@[ -d build_gcc_asan_ubsan ] && ninja -C build_gcc_asan_ubsan -q clean || true
	-@[ -d build_clang_release ] && ninja -C build_clang_release -q clean || true
	-@[ -d build_clang_debug ] && ninja -C build_clang_debug -q clean || true
	-@[ -d build_clang_tsan ] && ninja -C build_clang_tsan -q clean || true
	-@[ -d build_clang_asan_ubsan ] && ninja -C build_clang_asan_ubsan -q clean || true

reallyclean:
	rm -rf build_gcc_release build_gcc_debug build_gcc_tsan build_gcc_asan_ubsan \
	       build_clang_release build_clang_debug build_clang_tsan build_clang_asan_ubsan

