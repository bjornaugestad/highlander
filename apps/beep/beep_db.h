#ifndef BEEP_DB_H
#define BEEP_DB_H

#include <stddef.h>
#include <stdint.h>
#include <meta_common.h>
#include <tcp_client.h>

// Going old-school here, ditching flatcc, xdr, and what else.
// We can always change our minds later.
// Note that this file is the truth file!
// boa@20251122
//

// FIRST OF ALL, so we never forget: Message IDs. Each server request
// has an ID which must be unique and will be used by the dispatcher
// to locate the handler function.
#define BEEP_USER_ADD 0x01
#define BEEP_USER_DEL 0x02
#define BEEP_USER_UPD 0x03
#define BEEP_USER_GET 0x04


// Some constants
#define NAME_MAX 50
#define EMAIL_MAX 500
#define NICK_MAX NAME_MAX
#define TEXT_MAX 1023

// Some datatypes
typedef uint64_t dbid_t;
typedef char name_t[NAME_MAX + 1];
typedef char email_t[EMAIL_MAX + 1];
typedef char nick_t[NICK_MAX + 1];
typedef char text_t[TEXT_MAX + 1];


typedef struct user_tag {
    dbid_t id;
    name_t name;
    nick_t nick;
    email_t email;
} *User;

// Some user functions
User user_new(void);
void user_free(User p);

// Setters
void user_set_id(User, dbid_t);
void user_set_name(User, const char *);
void user_set_nick(User, const char *);
void user_set_email(User, const char *);

// Getters
dbid_t         user_id(User);
const char * user_name(User);
const char * user_nick(User);
const char * user_email(User);

// serializing functions
size_t user_size_t(User);

// Dispatcher stuff and messages
status_t user_add(User, tcp_client clnt);
status_t user_del(User);
status_t user_get(name_t name, User);
status_t user_update(User);

// Now let us add a new entity, Posts. We want to evaluate 1:n relations
typedef struct post_tag {
    dbid_t id;
    dbid_t user_id;
    text_t text;
} Post;

// So we have one Post, but what's many Posts?
// Array? Linked list? That's what we're trying to find out. If we lean into
// JSON syntax, we have something like [ Post{0,n} ] on the wire. When we
// read that in the client, we need to deal with n entries. We need a 
// container. list/dynamic array/realloc() array/. We care about clear code
// and good error handling, and reusability.

int post_add(User, text_t text);
int user_get_posts(User, Post);

#endif
