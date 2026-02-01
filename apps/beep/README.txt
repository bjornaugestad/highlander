beep - conceptual test of highlander with real database(libdb 5.3)

intro: we need to go beyond helloworld and run a real application.
We therefore add a database and implement an app using HTML/CSS
without JS(think Pitch) to do 'something'. Something's not defined ATM,
but we aim for a typical webapp with users and posts and images,
kinda like Pitch and Twitter.

Remember: this is technology research, not a final product. We keep
it simple. 


Why libdb(BerkeleyDB)? Because C-ISAM isn't available...
We want an in-process db we can link with, with multiple writers and readers,
multithread support, multiprocess support, transaction support,
and no sql. bdb fits the bill. bdb's owner sucks, so we use an
older version. We really should use C-ISAM, but it doesn't exist as open source
so what's a man to do? LMDB is not an option, RocksDB is C++.
libdb is the only option we have. Or is it? LLM+aider(.chat) can maybe
be used to generate a new C-ISAM? I really like C-ISAM. How hard is it
to implement a self-balancing B+ tree? 

WHAT TO BUILD?
--------------
a) A standalone db-server with a known API.
b) A client library
c) test programs

All comms will be over TLS1.3. We can establish connection pools at startup
and just keep connections between client and server persistent(for performance reasons).

SERIALIZING:
------------
20251126: Just wrote our own ;-) See cbuf.c. Need to add some semantic parsing eventually,
but that's no problemo.

LIFE WITH BERKELEYDB:
---------------------
Apparantly, we need a checkpoint thread running to checkpoint the db at regular intervals.
No worries, but we want proper shutdown semantics and error handling. Also, we want
to run everything by meta_process. 

What will the process look like? Well, it'll have n worker threads serving db requests
from clients. That's pretty much it. It shuts down on SIGTERM and has a checkpoint 
thread too. Very similar to the tcp_server architecture, but the connections will be
persistent so the client can do more than one request per TLS handshake. So we have
a connection pool like in the echo server. TBH, we'll do just what the echo server does,
except that we will serialize RPC requests and responses.

The meta_process object will manage both the TCP server and a (new) DB server object.
This way we can share shutdown semantics and even root resource management if needed.
The bdb_server will only have one thread though. Calls to the library will be done
from connection threads. By all means, bdb_server may have more than the checkpoint thread
eventually, but right now I only see the need for one thread.

So, in order to integrate bdb with meta_process, we need four functions:
do():
    setup environment
    open databases
    start checkpoint thread

undo():
    undo whatever do() did in case of failure

run():
    Not much really, since calls come from other threads
    AFAICT, just wait for shutdown. We may even be able to just return 0 so that
    start_one_service() is happy.

shutdown():
    stop checkpoint thread
    close databases
    close env


CLIENTS:

We will also need a client library and some client programs to test serializing and certs.
That lib will not see bdb, just tcp_client and serializer.




