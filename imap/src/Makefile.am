# IMAP Secure Daemon Debug. imapsd (non-debug)
bin_PROGRAMS=imapsdd imapsd

imapsdd_SOURCES=\
	imap.c

imapsd_SOURCES=$(imapsdd_SOURCES)

imapsd_LDADD=-lmeta -lpthread -lssl -lcrypto
imapsdd_LDADD=-lmeta -lpthread -lssl -lcrypto

imapsd_CFLAGS=-O0 -ggdb -Wall -Wextra -pedantic -std=gnu99
imapsdd_CFLAGS=-O3 -DNDEBUG -Wall -Wextra -pedantic -std=gnu99
