bin_PROGRAMS=foo 
foo_SOURCES=main.c hippapp.html 

nodist_foo_SOURCES=mypages.h hippapp.c 

food_SOURCES=$(foo_SOURCES)
nodist_food_SOURCES=$(nodist_foo_SOURCES)

foo_LDFLAGS=-L$(HOME)/lib 
foo_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread

%.c : %.html
	hipp -i mypages.h -o $*.c $<

mypages.h: hippapp.html 
	hipp -pi $@ $+

BUILT_SOURCES=$(nodist_foo_SOURCES)
CLEANFILES=$(nodist_foo_SOURCES)

