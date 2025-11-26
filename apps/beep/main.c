#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <db.h>

// Checkpoint each minute. 
// Code's from the Core-C-Txn.pdf and needs more love eventually.
// meta_process FTW
static void *checkpoint_thread(void *varg)
{
    DB_ENV *dbenv = varg;
    int ret;

    puts("Right here, bro");
    for (;; sleep(60)) {
        ret = dbenv->txn_checkpoint(dbenv, 0, 0, 0);
        if (ret != 0) {
            dbenv->err(dbenv, ret, "checkpoint thread");
            exit(1);
        }
    }

    return NULL;
}

// Now how do we organize our databases(tables)? We need to refer to them 
// by some ID. 
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
};

int main(void)
{
    // We need a DB Environment since we want transactions et.al.
    DB_ENV *envp = NULL;
    const char *db_homedir = ".";

    int ret = db_env_create(&envp, 0);
    if (ret != 0) {
        fprintf(stderr, "Could not create ENV: %s\n", db_strerror(ret));
        return EXIT_FAILURE;
    }

    // Set up the environment
    uint32_t env_flags = DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_THREAD | DB_INIT_TXN;
    ret = envp->open(envp, db_homedir, env_flags, 0);
    if (ret != 0) {
        fprintf(stderr, "Could not open ENV: %s\n", db_strerror(ret));
        return EXIT_FAILURE;
    }

    // Now we seem to need to deal with checkpoint ourselves.
    pthread_t tid;
    ret = pthread_create(&tid, NULL, checkpoint_thread, envp);
    if (ret != 0) {
        fprintf(stderr, "failed to create checkpoint thread:%s\n", strerror(ret));
        exit(1);
    }

    // Open the databases
    size_t i, n = sizeof databases / sizeof *databases;
    for (i = 0; i < n; i++) {
        struct database *p = databases + i;
        ret = db_create(&p->dbp, envp, 0);
        if (ret != 0) {
            fprintf(stderr, "Could not create db: %s\n", db_strerror(ret));
            return EXIT_FAILURE;
        }

        ret = p->dbp->open(p->dbp, p->transaction_pointer, p->diskfile, NULL, p->access_method, p->openflags, p->filemode);
        if (ret != 0) {
            fprintf(stderr, "Could not open db: %s\n", db_strerror(ret));
            return EXIT_FAILURE;
        }
    }

    // Close all databases
    for (i = 0; i < n; i++) {
        struct database *p = databases + i;
        if (p->dbp != NULL)
            p->dbp->close(p->dbp, 0);
    }

    // Close it all down
    ret = envp->close(envp, 0);
    if (ret != 0)
        fprintf(stderr, "env close failed: %s\n", db_strerror(ret));


    return !!ret;
}
