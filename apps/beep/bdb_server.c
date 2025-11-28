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

    // Databases
    db_user users;
};


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

        sleep(1);
    }

    return NULL;
}

static status_t open_databases(bdb_server p)
{
    assert(p != NULL);

    if (!db_user_open(p->users, p->envp))
        return failure;

    return success;
}

static status_t close_databases(bdb_server p)
{
    assert(p != NULL);
    assert(p->users != NULL);

    if (!db_user_close(p->users))
        return failure;

    return success;
}

// Set up the environment
static const uint32_t env_flags = DB_CREATE 
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
    bdb_server new = malloc(sizeof *new);
    if (new == NULL)
        return NULL;

    new->users = db_user_new();
    if (new->users == NULL) {
        free(new);
        return NULL;
    }

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
        db_user_free(this->users);
        free(this);
    }
}


// When's a user valid for insertion? We need a name and a nick, which
// should be unique too. Let's just see if insert fails due to duplicates
// instead of creating a race cond by checking here.
static bool user_valid_for_insert(User u)
{
    assert(u != NULL);
    if (strlen(user_name(u)) == 0
    ||  strlen(user_nick(u)) == 0
    ||  strlen(user_email(u)) == 0)
        return false;

    if (user_id(u) != 0)
        return false;

    return true;
}

// Notes from the western front: Bdb is harder than RMDBS
// We want to insert one row in the users table
// We need a DB_SEQUENCE for the users table
// We need two secondary databases for the two unique columns(name, nick), perhaps even three(email)
// 
// No worries: We just need to add whatever to the databases table, and use associate() to
// connect stuff. Here we just need to
// a) grab db pointers for each db
// b) start a transaction 
// c) get a new sequence id, insert row, update sequence db, commit.
//
// First time is always the hardest... ;)
// 
// On sequences: They're special. Check out /usr/share/doc/libdb-devel/examples/c/ex_sequence.c.
// The gist is that they work with db_seq_t numbers, are created with db_sequence_create(),
// and we should probably use one per unique PK. So users_sequence for users.db, and so forth.
// NOTE that db_sequence_create() wants a DB*. That's not the users.db in our case, but a "users_sequence.db"
//
//
// On secondary databases: Read more in the tutorial first. It's messy. Not hard, just not elegant.
status_t bdb_user_add(User u)
{
    assert(u != NULL);
    if (!user_valid_for_insert(u))
        return failure;

    (void)u;

    return success;
}

