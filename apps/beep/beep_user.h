#ifndef BEEP_USER_H
#define BEEP_USER_H

#include <meta_common.h>
#include <connection.h>

#include <beep_datatypes.h>

typedef struct user_tag *User;

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
status_t user_add(User, connection clnt);
status_t user_del(User);
status_t user_get(name_t name, User);
status_t user_update(User);


#endif

