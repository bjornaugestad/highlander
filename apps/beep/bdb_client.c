// bdb client, wtf is it?
// It will be a web server which is a client to the bdb server, but ATM it's just
// a  CLI tool to test the misc APIs (APIs is a fucking n00b term, but yeah).
//
// bdb client will become a set of C functions which will marshal data and create
// the header and send it to the server for parsing, as well as parse the data
// returned from the server. This client will share ALL definitions of structs
// with the server. The goal is to build a client lib consisting og functions
// like user_add(), user_del(), et al., including user_get_new_comments() and
// similar.
//
// IOW, this file will implement functions for all functionality in bdb_server,
// as well as deal with byte ordering, serializing, and what else. It will
// also handle persistent connections to the server, as well as TLS and keys/certs.
//
// So what do we need to establish it all? We need 
// - a pool of connections to the server 
// - a set of operations (ins, del, get, upd)
// - marshalling:
//
// So what to test? user_add() ? What's user_add() anyways? 
// It's basically just an insert to the user 'database', but it's marshalled
// and uses a header. We need to get NBO correct, as well as request id for the dispatcher.
// But where's User defined? ATM, it's defined in beep_db.h as User, along with access function.
// What we're looking for ATM, is a scalable pattern. marshal(User), unmarshal(User), store(User),
// get(User), del(User), and so forth. The point is not du deal with User, but to create a
// reusable pattern for other ADTs.

// Where to start? user_add()? Kina makes sense. Still, we need the framework up and running,
// as in threadpools with persistent connections, auth, versioning, and shared struct definitions.

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <tcp_client.h>
#include <cbuf.h>
#include <beep_db.h>
#include <beep_user.h>

int main(void)
{
    tcp_client clnt = tcp_client_new(SOCKTYPE_TCP);
    //tcp_client_set_hostname(clnt, "localhost");
    //tcp_client_set_port(clnt, 3000);

    status_t rc = tcp_client_connect(clnt, "::1", 3000);
    if (rc != success)
        die("Failed to connect to server");

    User u = user_new();
    user_set_name(u, "hello");
    user_set_nick(u, "world");
    user_set_email(u, "wtf@foo.com");

    connection conn = tcp_client_connection(clnt);

    rc = user_add(u, conn);


    user_free(u);
    tcp_client_close(clnt);
    tcp_client_free(clnt);
    return 0;
}

