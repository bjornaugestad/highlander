#ifndef BEEP_DB_USER_H
#define BEEP_DB_USER_H

#include <db.h>

// Just functions for secondary databases
int get_user_name(DB *dbnamep, const DBT *pkey, const DBT *pdata, DBT *namep_key);
int get_user_nick(DB *dbnickp, const DBT *pkey, const DBT *pdata, DBT *nickp_key);
int get_user_email(DB *dbemailp, const DBT *pkey, const DBT *pdata, DBT *emailp_key);

#endif

