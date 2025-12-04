#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <db.h>

#include <meta_common.h>

#include <bdb_server.h>
#include <db_user.h>

struct bdb_server_tag {
    DB_ENV *envp; 
    const char *homedir;
    pthread_t checkpoint_tid;
    _Atomic bool shutting_down;

    // Some stats
    size_t ncommits, ncommit_failures;
    size_t nrollbacks, nrollback_failures;
};

// Lots of databases. One primary, dbp, three secondary ones for unique constraints,
// and finally a place for the user sequence to be stored.
// 20251202: We need a unified way of dealing with databases, as there will be lots of them.
// So we're back to the good old database struct and will add functions do deal with them.
// 20251204: Since we must open all or nothing when starting, we merge all db_create/open/
// db_sequence_create, accociate. So no longer per database, now we go per *server*.
struct database {
    int id;
    DB *dbp; 
    void *txp;                   // Transaction pointer
    const char *diskfile; 
    const char *logical_db_name; // Optional
    int access;     // BTREE/QUEUE/HASH
    uint32_t flags;
    int mode; // File's mode on disk (think umask)
};

static struct database databases[] = {
    // user
    { DB_USER_USER,     NULL, NULL, "users.db",          NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { DB_USER_NAME,     NULL, NULL, "users_name.db",     NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { DB_USER_NICK,     NULL, NULL, "users_nick.db",     NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { DB_USER_EMAIL,    NULL, NULL, "users_email.db",    NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { DB_USER_SEQUENCE, NULL, NULL, "users_sequence.db", NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },

    // sub(reddits)
    { DB_SUBS_SUB,      NULL, NULL, "subs.db",           NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
};

static struct sequence {
    int dbid, seqid;
    DB_SEQUENCE *seq;
    DBT key;
    char name[50];
} sequences[] = {
    { DB_USER_USER, DB_USER_SEQUENCE, NULL, {0, }, "user_sequence" },
};


DB *bdb_server_get_db(bdb_server p, int id)
{
    (void)p;
    assert(id >= 0);
    assert((size_t)id < sizeof databases / sizeof *databases);

    return databases[id].dbp;
}

DB_SEQUENCE* bdb_server_get_sequence(bdb_server p, int id)
{
    (void)p;
    assert(id >= 0);

    size_t i, n = sizeof sequences / sizeof *sequences;
    for (i = 0; i < n; i++) {
        if (sequences[i].seqid == id)
            return sequences[i].seq;
    }

    return NULL;
}

// Checkpoint from time to time.
// Code's from the Core-C-Txn.pdf and needs more love eventually.
// meta_process FTW
static void *checkpoint_thread(void *varg)
{
    bdb_server srv = varg;

    while (!srv->shutting_down) {
        int ret = srv->envp->txn_checkpoint(srv->envp, 0, 0, 0);
        if (ret != 0) {
            srv->envp->err(srv->envp, ret, "checkpoint thread");
            exit(1);
        }

        sleep(5);
    }

    return NULL;
}

// create and open the databases, sequences and associations
static status_t open_databases(bdb_server p)
{
    assert(p != NULL);

    size_t i, n = sizeof databases / sizeof *databases;
    for (i = 0; i < n; i++) {
        int ret = db_create(&databases[i].dbp, p->envp, 0);
        if (ret != 0)
            return failure;
    }

    for (i = 0; i < n; i++) {
        int ret = databases[i].dbp->open(databases[i].dbp, NULL, databases[i].diskfile,
            databases[i].logical_db_name, databases[i].access, databases[i].flags,
            databases[i].mode);

        if (ret != 0)
            return failure;
    }

    // Associate primary db with secondary ones. 
    databases[DB_USER_USER].dbp->associate(databases[DB_USER_USER].dbp, NULL, databases[DB_USER_NAME].dbp,  get_user_name,  DB_AUTO_COMMIT);
    databases[DB_USER_USER].dbp->associate(databases[DB_USER_USER].dbp, NULL, databases[DB_USER_NICK].dbp,  get_user_nick,  DB_AUTO_COMMIT);
    databases[DB_USER_USER].dbp->associate(databases[DB_USER_USER].dbp, NULL, databases[DB_USER_EMAIL].dbp, get_user_email, DB_AUTO_COMMIT);

    // Now sequences. They're a bit silly as they're databases, but 'special' ones.
    // They're created with db_create() and are open() already, but we need some extra magic words
    // The database is more like a backing store for the sequences. We have a separate table for
    // the actual sequence of type DB_SEQUENCE.

    n = sizeof sequences / sizeof *sequences;
    for (i = 0; i < n; i++) {
        memset(&sequences[i].key, 0, sizeof sequences[i].key);
        sequences[i].key.data = sequences[i].name;
        sequences[i].key.size = (uint32_t)strlen(sequences[i].name) + 1;
        int ret = db_sequence_create(&sequences[i].seq, databases[sequences[i].seqid].dbp, 0);
        if (ret != 0) {
            return failure;
        }

        ret = sequences[i].seq->open(sequences[i].seq, NULL, &sequences[i].key, DB_CREATE | DB_AUTO_COMMIT);
        if (ret != 0) {
            return failure;
        }
    }
    return success;
}

static status_t close_databases(bdb_server p)
{
    assert(p != NULL);

    // Close sequences before databases
    size_t i, n = sizeof sequences / sizeof *sequences;
    for (i = 0; i < n; i++) 
        sequences[i].seq->close(sequences[i].seq, 0);

    // Close databases in reverse order, iow primary db last.
    n = sizeof databases / sizeof *databases;
    while (n) {
        struct database *db = &databases[--n]; 
        db->dbp->close(db->dbp, 0);
        db->dbp = NULL;
    }

    (void)p;
    return success;
}

// Set up the environment
static const uint32_t env_flags 
    = DB_CREATE 
    | DB_INIT_LOCK 
    | DB_INIT_LOG 
    | DB_INIT_MPOOL 
    | DB_THREAD 
    | DB_INIT_TXN;

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

    p->envp->set_flags(p->envp, DB_TXN_WRITE_NOSYNC, 1);

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
// So kill checkpoint thread, close all databases, close env. 
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

    return ret == 0 ? success : failure;
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
    bdb_server new = calloc(1, sizeof *new);
    if (new == NULL)
        return NULL;

    new->homedir = ".";
    new->checkpoint_tid = 0;
    new->shutting_down = false;
    new->envp = NULL;

    return new;
}

void bdb_server_free(bdb_server this)
{
    if (this != NULL) {
        // Semantics: Do we want the dtor to close everything? Not really
        free(this);
    }
}


DB_TXN* bdb_server_begin(bdb_server p)
{
    assert(p != NULL);
    DB_TXN *txn = NULL;
    int ret = p->envp->txn_begin(p->envp, NULL, &txn, 0);
    if (ret != 0)
        return NULL;

    return txn;
}

status_t bdb_server_commit(bdb_server p, DB_TXN *txn)
{
    assert(p != NULL);
    assert(txn != NULL);

    int ret = txn->commit(txn, 0);
    if (ret == 0) {
        p->ncommits++;
        return success;
    }

    p->ncommit_failures++;
    return failure;
}


status_t bdb_server_rollback(bdb_server p, DB_TXN *txn)
{
    assert(p != NULL);
    assert(txn != NULL);

    int ret = txn->abort(txn);
    if (ret == 0) {
        p->nrollbacks++;
        return success;
    }

    p->nrollback_failures++;
    return failure;
}

