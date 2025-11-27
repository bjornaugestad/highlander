#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <beep_user.h>
#include <cbuf.h>

struct user_tag {
    dbid_t id;
    name_t name;
    nick_t nick;
    email_t email;
};

User user_new(void)
{
    User new = malloc(sizeof *new);
    if (new == NULL)
        return NULL;

    new->id = 0;
    new->name[0] = '\0';
    new->nick[0] = '\0';
    new->email[0] = '\0';
    return new;
}

void user_free(User u)
{
    if (u) {
        free(u);
    }
}

size_t user_size_t(User u)
{
    return sizeof *u;
}

void user_set_id(User u, dbid_t id)
{
    assert(u != NULL);
    // assert(id != 0); // reserved
    u->id = id;
}

void user_set_name(User u, const char *val)
{
    assert(u != NULL);
    assert(val != NULL);

    size_t n = strlen(val);
    assert(n < sizeof u->name);
    memcpy(u->name, val, n + 1);
}

void user_set_nick(User u, const char *val)
{
    assert(u != NULL);
    assert(val != NULL);

    size_t n = strlen(val);
    assert(n < sizeof u->nick);
    memcpy(u->nick, val, n + 1);
}

void user_set_email(User u, const char *val)
{
    assert(u != NULL);
    assert(val != NULL);

    size_t n = strlen(val);
    assert(n < sizeof u->email);
    memcpy(u->email, val, n + 1);
}

dbid_t user_id(User u)
{
    assert(u != NULL);
    return u->id;
}

const char * user_name(User u)
{
    assert(u != NULL);
    return u->name;
}

const char * user_nick(User u)
{
    assert(u != NULL);
    return u->nick;
}

const char * user_email(User u)
{
    assert(u != NULL);
    return u->email;
}


// Send the user to the server for insertion into the database.
// We need a version, a request id, as well as payload.
status_t user_add(User u, connection conn)
{
    assert(u != NULL);

    struct beep_header h = {BEEP_VERSION, BEEP_USER_ADD};

    if (!writebuf_header(conn, &h))
        return failure;

    if (!writebuf_object_start(conn)
    ||  !writebuf_uint64(conn, user_id(u))
    ||  !writebuf_string(conn, user_name(u))
    ||  !writebuf_string(conn, user_nick(u))
    ||  !writebuf_string(conn, user_email(u))
    ||  !writebuf_object_end(conn))
        return failure;

    if (!connection_flush(conn))
        die("Could not flush connection");

    sleep(4);

    // Now read a reply just to see if we're non-blocking or not.
    char buf[1024];
    ssize_t nread = connection_read(conn, buf, sizeof buf);
    if (nread < 0)
        die("error reading from server\n");

    return success;
}

