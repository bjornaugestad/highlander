#ifndef BEEP_DB_USER_H
#define BEEP_DB_USER_H

#include <db.h>

#include <meta_common.h>
#include <beep_user.h>

// This is the interface to the user table/database.
typedef struct db_user *db_user;

db_user db_user_new(void);
void db_user_free(db_user u);

status_t db_user_open(db_user u, DB_ENV *envp);
status_t db_user_close(db_user u);

dbid_t db_user_ins(db_user dbu, User u);


#endif

