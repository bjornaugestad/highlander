beep - conceptual test of highlander with real database(libdb 5.3)

intro: we need to go beyond helloworld and run a real application.
We therefore add a database and implement an app using HTML/CSS
without JS(think Pitch) to do 'something'. Something's not defined ATM,
but we aim for a typical webapp with users and posts and images,
kinda like Pitch and Twitter.

Remember: this is technolory research, not a final product. We keep
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
THat's a big one and I have no good answer. We wanna do RPC, but rpcgen


