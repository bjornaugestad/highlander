CFLAGS=-Wall -Wextra -pedantic -std=gnu23 -I $(HOME)/src/highlander/meta/src -O0 -ggdb

all: beep cbuf_check

beep: main.o cbuf.o
	$(CC) $(CFLAGS) -o $@ $+ -ldb -lpthread

cbuf_check: cbuf.c user.c $(HOME)/src/highlander/meta/src/meta_common.c
	$(CC) -DCBUF_CHECK $(CFLAGS) -o $@ $+ 

clean:
	rm -f *.o cbuf_check
