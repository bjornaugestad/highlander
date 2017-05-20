bin_PROGRAMS=foo
foo_SOURCES=main.c hippapp.html 

nodist_foo_SOURCES=mypages.h hippapp.c 
foo_CFLAGS=-W -Wall -pedantic -Wextra -std=gnu99 -Wshadow -Wmissing-prototypes -pthread -I $(HOME)/include
foo_LDFLAGS=-L$(HOME)/lib
foo_LDADD=-lhighlanderd -lmetad -lssl -lcrypto -lpthread

%.c : %.html
	hipp -i mypages.h -o $*.c $<

mypages.h: hippapp.html 
	hipp -pi $@ $+

BUILT_SOURCES=$(nodist_foo_SOURCES)
CLEANFILES=$(nodist_foo_SOURCES)

