AM_CFLAGS=-ggdb -O0 -Wall -Wextra -pedantic -std=gnu11 -Werror -pthread\
	-Iinclude -I$(top_srcdir)/include\
	-Wno-empty-body

include_HEADERS=\
	include/metal.h\
	include/metal_limits.h

bin_PROGRAMS=metalsh
lib_LIBRARIES=libmetal.a
metalsh_CFLAGS=$(AM_CFLAGS)
metalsh_LDADD=-L. -lmetal -lmeta -lpthread
metalsh_SOURCES=\
	src/apps/metalsh.c

libmetal_a_SOURCES=\
	src/kernel/tasks.c\
	src/kernel/task.c\
	src/video/webcam_task.c

TESTS=\
	tasks_check\
	helloworld\
	pingpong

check_PROGRAMS=\
	tasks_check\
	helloworld\
	pingpong


tasks_check_SOURCES=\
	src/kernel/task.c\
	src/kernel/tasks.c

tasks_check_CFLAGS=-DTASKS_CHECK -I$(top_srcdir)/include -ggdb -O0
tasks_check_LDADD=-lmeta -lpthread

helloworld_SOURCES=\
	src/tests/helloworld.c\
	src/kernel/task.c\
	src/kernel/tasks.c

helloworld_CFLAGS=-I$(top_srcdir)/include -ggdb -O0
helloworld_LDADD=-lmeta -lpthread

pingpong_SOURCES=\
	src/tests/pingpong.c\
	src/kernel/task.c\
	src/kernel/tasks.c

pingpong_CFLAGS=-I$(top_srcdir)/include -ggdb -O0
pingpong_LDADD=-lmeta -lpthread

