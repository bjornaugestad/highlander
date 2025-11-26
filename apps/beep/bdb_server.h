#ifndef BEEP_BDBSERVER_H
#define BEEP_BDBSERVER_H

#include <meta_common.h>

typedef struct bdb_server_tag *bdb_server;

bdb_server bdb_server_new(void);
void bdb_server_free(bdb_server p);

status_t bdb_server_do_func(void *v);
status_t bdb_server_undo_func(void *v);
status_t bdb_server_run_func(void *v);
status_t bdb_server_shutdown_func(void *v);

#endif

