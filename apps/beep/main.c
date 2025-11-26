#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <meta_process.h>
#include <bdb_server.h>

int main(void)
{
    bdb_server db = bdb_server_new();

    process p = process_new("beep");

    status_t rc = process_add_object_to_start(p, db, 
        bdb_server_do_func,
        bdb_server_undo_func,
        bdb_server_run_func,
        bdb_server_shutdown_func);

    if (rc != success)
        die("process_add_object_to_start");

    if (!process_start(p, 0))
        die("process_start()");

    if (!process_wait_for_shutdown(p))
        die("process_wait_for_shutdown");

    bdb_server_free(db);
    process_free(p);
    return 0;
}
