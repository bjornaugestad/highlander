#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "beep_db.h"

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

void user_set_id(User u, dbid_t id)
{
    assert(u != NULL);
    assert(id != 0); // reserved
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


