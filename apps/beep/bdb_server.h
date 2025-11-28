#ifndef BEEP_BDBSERVER_H
#define BEEP_BDBSERVER_H

#include <meta_common.h>

#include <beep_user.h>


// Stuff we need to integrate bdb_server with meta_process
typedef struct bdb_server_tag *bdb_server;

bdb_server bdb_server_new(void);
void bdb_server_free(bdb_server p);

status_t bdb_server_do_func(void *v);
status_t bdb_server_undo_func(void *v);
status_t bdb_server_run_func(void *v);
status_t bdb_server_shutdown_func(void *v);

// Insert a new User record.
dbid_t bdb_user_add(bdb_server srv, User u);

#endif

