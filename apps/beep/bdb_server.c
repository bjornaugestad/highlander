#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <db.h>

#include <meta_common.h>

#include <bdb_server.h>

// Now how do we organize our databases(tables)? We need to refer to them by some ID. 
// Sentinel value: 0
#define DB_USERS 0x01
static struct database {
    int id;
    DB *dbp;
    void *transaction_pointer;
    uint32_t openflags;
    const char *diskfile;
    uint32_t access_method;
    int filemode;
} databases[] = {
    { DB_USERS, NULL, NULL, DB_CREATE, "users.db", DB_BTREE, 0 },
    { 0, NULL, NULL, 0, 0, DB_BTREE, 0 },
};

struct bdb_server_tag {
    DB_ENV *envp; 
    struct database *db; // Pointer to the table above
    const char *homedir;
    pthread_t checkpoint_tid;
    _Atomic bool shutting_down;
};


// Checkpoint from time to time.
// Code's from the Core-C-Txn.pdf and needs more love eventually.
// meta_process FTW
static void *checkpoint_thread(void *varg)
{
    bdb_server srv = varg;
    int ret;

    puts("Right here, bro");
    while (!srv->shutting_down) {
        ret = srv->envp->txn_checkpoint(srv->envp, 0, 0, 0);
        if (ret != 0) {
            srv->envp->err(srv->envp, ret, "checkpoint thread");
            exit(1);
        }

        sleep(1);
    }

    return NULL;
}

static status_t open_databases(bdb_server p)
{
    // Open the databases
    for (size_t i = 0; p->db[i].id != 0; i++) {
        int ret = db_create(&p[i].db->dbp, p->envp, 0);
        if (ret != 0) {
            fprintf(stderr, "Could not create db: %s\n", db_strerror(ret));
            return failure;
        }

        ret = p[i].db->dbp->open(p[i].db->dbp, p[i].db->transaction_pointer, p[i].db->diskfile, NULL, p[i].db->access_method, p[i].db->openflags, p[i].db->filemode);
        if (ret != 0) {
            fprintf(stderr, "Could not open db: %s\n", db_strerror(ret));
            return failure;
        }
    }
    
    return success;
}

static status_t close_databases(bdb_server p)
{
    // Close all databases
    for (size_t i = 0; p->db[i].id != 0; i++) {
        if (p->db[i].dbp != NULL)
            p->db[i].dbp->close(p->db[i].dbp, 0);
    }

    return success;
}

status_t bdb_server_do_func(void *v)
{
    // We need a DB Environment since we want transactions et.al.
    bdb_server p = v;
    assert(p != NULL);

    p->envp = NULL;
    p->checkpoint_tid = 0;

    int ret = db_env_create(&p->envp, 0);
    if (ret != 0)
        return failure;

    // Set up the environment
    const uint32_t env_flags = DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_THREAD | DB_INIT_TXN;
    ret = p->envp->open(p->envp, p->homedir, env_flags, 0);
    if (ret != 0)
        return failure;

    // Now we seem to need to deal with checkpoint ourselves.
    ret = pthread_create(&p->checkpoint_tid, NULL, checkpoint_thread, p);
    if (ret != 0)
        return failure;

    // Open all databases
    return open_databases(p);
}

// If do_func() failed, we need to reverse whatever do_func() did
// So kill checkpoint thread, close all databases, close env. Alternative is to
// have do_func() clean up after itself. Nice idea, but it doesn't fit multi-object process management.
// Another object may fail even if this do_func() succeeded.
status_t bdb_server_undo_func(void *v)
{
    bdb_server p = v;

    // Stop the checkpoint thread
    p->shutting_down = true;
    pthread_join(p->checkpoint_tid, NULL);

    close_databases(p);

    // Close the environment too
    int ret = p->envp->close(p->envp, 0);
    if (ret != 0)
        fprintf(stderr, "env close failed: %s\n", db_strerror(ret));

    return success;
}

status_t bdb_server_run_func(void *v)
{
    bdb_server p = v;
    (void)p;
    return success;
}

status_t bdb_server_shutdown_func(void *v)
{
    bdb_server p = v;
    p->shutting_down = true;
    pthread_join(p->checkpoint_tid, NULL);


    // clean up, as in close stuff
    close_databases(p);

    // Close the environment too
    int ret = p->envp->close(p->envp, 0);
    if (ret != 0)
        fprintf(stderr, "env close failed: %s\n", db_strerror(ret));

    return success;
}

                 
bdb_server bdb_server_new(void)
{
    bdb_server new = malloc(sizeof *new);
    if (new != NULL) {
        new->db = databases;
        new->homedir = ".";
        new->checkpoint_tid = 0;
        new->shutting_down = false;
        new->envp = NULL;
    }

    return new;
}



void bdb_server_free(bdb_server this)
{
    if (this != NULL) {
        // Semantics: Do we want the dtor to close everything? Not really
        free(this);
    }
}
