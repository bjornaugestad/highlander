#ifndef BEEP_BDBSERVER_H
#define BEEP_BDBSERVER_H

#include <meta_common.h>

#include <beep_user.h>
#include <db_user.h>

// So for our user database, we need the following databases. Primary is always zero.
// Be precise for these are array indices!
#define DB_USER_USER     0x00   // primary db
#define DB_USER_NAME     0x01   // The secondary db
#define DB_USER_NICK     0x02   // also secondary db
#define DB_USER_EMAIL    0x03   // also secondary db
#define DB_USER_SEQUENCE 0x04   // The sequence database to generate user.id
#define DB_SUBS_SUB      0x05   // Top-level sub db


// Stuff we need to integrate bdb_server with meta_process
typedef struct bdb_server_tag *bdb_server;

bdb_server bdb_server_new(void);
void bdb_server_free(bdb_server p);

status_t bdb_server_do_func(void *v);
status_t bdb_server_undo_func(void *v);
status_t bdb_server_run_func(void *v);
status_t bdb_server_shutdown_func(void *v);

// The bdb server manages transactions for us
DB_TXN* bdb_server_begin(bdb_server p);
status_t bdb_server_commit(bdb_server p, DB_TXN *txn);
status_t bdb_server_rollback(bdb_server p, DB_TXN *txn);

// Insert a new User record.
dbid_t bdb_user_add(bdb_server srv, User u);
DB *bdb_server_get_db(bdb_server p, int id);

// Here we want the id of e.g. DB_USER_SEQUENCE, not DB_USER_USER
DB_SEQUENCE* bdb_server_get_sequence(bdb_server p, int id);

#endif

