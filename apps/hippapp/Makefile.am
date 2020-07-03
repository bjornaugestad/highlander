# build both debug and non-debug versions
bin_PROGRAMS=foo food
foo_SOURCES=main.c hippapp.html 

nodist_foo_SOURCES=mypages.h hippapp.c 

food_SOURCES=$(foo_SOURCES)
nodist_food_SOURCES=$(nodist_foo_SOURCES)

foo_CFLAGS=-W -Wall -pedantic -Wextra -std=gnu2x -Wshadow -Wmissing-prototypes -pthread -O3 -DNDEBUG
foo_LDFLAGS=-L/usr/local/lib -L$(HOME)/lib # Assuming local install of highlander
foo_LDADD=-lhighlander -lmeta -lssl -lcrypto -lpthread

food_CFLAGS=-W -Wall -pedantic -Wextra -std=gnu2x -Wshadow -Wmissing-prototypes -pthread -O0 -ggdb
food_LDFLAGS=-L/usr/local/lib -L$(HOME)/lib # Assuming local install of highlander
food_LDADD=-lhighlanderd -lmetad -lssl -lcrypto -lpthread

%.c : %.html
	hipp -i mypages.h -o $*.c $<

mypages.h: hippapp.html 
	hipp -pi $@ $+

BUILT_SOURCES=$(nodist_foo_SOURCES)
CLEANFILES=$(nodist_foo_SOURCES)

