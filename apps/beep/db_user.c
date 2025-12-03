#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <bdb_server.h>
#include <db_user.h>

// Lots of databases. One primary, dbp, three secondary ones for unique constraints,
// and finally a place for the user sequence to be stored.
// 20251202: We need a unified way of dealing with databases, as there will be lots of them.
// So we're back to the good old database struct and will add functions do deal with them.

struct database {
    DB *dbp; 
    void *txp;                   // Transaction pointer
    const char *diskfile; 
    const char *logical_db_name; // Optional
    int access;     // BTREE/QUEUE/HASH
    uint32_t flags;
    int mode; // File's mode on disk (think umask)
};

// So for our user database, we need the following databases. Primary is always zero.
// Be precise for array indices!
#define DB_USER_USER     0x00   // primary db
#define DB_USER_NAME     0x01   // The secondary db
#define DB_USER_NICK     0x02   // also secondary db
#define DB_USER_EMAIL    0x03   // also secondary db
#define DB_USER_SEQUENCE 0x04   // The sequence database to generate user.id

static struct database user_databases[] = {
    { NULL, NULL, "users.db",          NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { NULL, NULL, "users_name.db",     NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { NULL, NULL, "users_nick.db",     NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { NULL, NULL, "users_email.db",    NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
    { NULL, NULL, "users_sequence.db", NULL, DB_BTREE, DB_AUTO_COMMIT | DB_CREATE | DB_THREAD, 0 },
};

struct db_user {
    struct database *dbs;
    size_t ndb;

    DB_SEQUENCE *seq;
    DBT seqkey;
};

db_user db_user_new(void)
{
    db_user new = calloc(1, sizeof *new);
    if (new == NULL)
        return NULL;

    new->dbs = user_databases;
    new->ndb = sizeof user_databases / sizeof *user_databases;
    return new;
}

void db_user_free(db_user u)
{
    if (u == NULL)
        return;

    free(u);
}

static int get_user_name(DB *dbnamep, const DBT *pkey, const DBT *pdata, DBT *namep_key)
{
    assert(dbnamep != NULL);
    assert(pkey != NULL);
    assert(pdata != NULL);
    assert(namep_key != NULL);

    User u = pdata->data;
    memset(namep_key, 0, sizeof *namep_key);

    char *name = (char *)user_name(u);
    namep_key->data = name;
    namep_key->size = (uint32_t)strlen(name) + 1;

    (void)dbnamep;
    (void)pkey;
    return 0;
}

static int get_user_nick(DB *dbnickp, const DBT *pkey, const DBT *pdata, DBT *nickp_key)
{
    assert(dbnickp != NULL);
    assert(pkey != NULL);
    assert(pdata != NULL);
    assert(nickp_key != NULL);

    User u = pdata->data;
    memset(nickp_key, 0, sizeof *nickp_key);

    char *nick = (char *)user_nick(u);
    nickp_key->data = nick;
    nickp_key->size = (uint32_t)strlen(nick) + 1;

    (void)dbnickp;
    (void)pkey;
    return 0;
}

static int get_user_email(DB *dbemailp, const DBT *pkey, const DBT *pdata, DBT *emailp_key)
{
    assert(dbemailp != NULL);
    assert(pkey != NULL);
    assert(pdata != NULL);
    assert(emailp_key != NULL);

    User u = pdata->data;
    memset(emailp_key, 0, sizeof *emailp_key);

    char *email = (char *)user_email(u);
    emailp_key->data = email;
    emailp_key->size = (uint32_t)strlen(email) + 1;

    (void)dbemailp;
    (void)pkey;
    return 0;
}

status_t db_user_open(db_user u, DB_ENV *envp)
{
    assert(u != NULL);
    int ret;

    // Create the databases
    for (size_t i = 0; i < u->ndb; i++) {
        ret = db_create(&u->dbs[i].dbp, envp, 0);
        if (ret)
            goto die;
    }

    // Now open the databases
    for (size_t i = 0; i < u->ndb; i++) {
        struct database *p = u->dbs + i;
        ret = p->dbp->open(p->dbp, NULL, p->diskfile, p->logical_db_name,
            p->access, p->flags, p->mode);

        if (ret)
            goto die;
    }

    // Associate primary db with secondary ones. 
    u->dbs[DB_USER_USER].dbp->associate(u->dbs[DB_USER_USER].dbp, NULL, u->dbs[DB_USER_NAME].dbp,  get_user_name,  DB_AUTO_COMMIT);
    u->dbs[DB_USER_USER].dbp->associate(u->dbs[DB_USER_USER].dbp, NULL, u->dbs[DB_USER_NICK].dbp,  get_user_nick,  DB_AUTO_COMMIT);
    u->dbs[DB_USER_USER].dbp->associate(u->dbs[DB_USER_USER].dbp, NULL, u->dbs[DB_USER_EMAIL].dbp, get_user_email, DB_AUTO_COMMIT);

    // Deal with the sequence too
    ret = db_sequence_create(&u->seq, u->dbs[DB_USER_SEQUENCE].dbp, 0);
    if (ret != 0)
        goto monster_err;

    static char seqname[] = "user_sequence"; // NOTE: lifespan looks fishy. Move to ADT?
    memset(&u->seqkey, 0, sizeof u->seqkey);
    u->seqkey.data = seqname;
    u->seqkey.size = sizeof seqname;
    ret = u->seq->open(u->seq, NULL, &u->seqkey, DB_CREATE | DB_AUTO_COMMIT);
    if (ret != 0)
        goto monster_err;

    return success;

die:
monster_err: // A hell of a lot to clean up
    fprintf(stderr, "Meh, hit monster err with ret == %d: %s\n", ret, db_strerror(ret));
    return failure;
}

status_t db_user_close(db_user u)
{
    assert(u != NULL);
    assert(u->dbs != NULL);

    // Close sequences before primary database
    u->seq->close(u->seq, 0);

    size_t n = u->ndb;
    while (n) {
        struct database *db = &u->dbs[--n];
        db->dbp->close(db->dbp, 0);
        u->dbs[n].dbp = NULL;
    }

    return success;
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

    if (user_created(u) != 0)
        return false;

    return true;
}

static timestamp now(void)
{
    return (timestamp)time(NULL);
}

dbid_t bdb_user_add(bdb_server srv, User u)
{
    int ret = 0;
    assert(u != NULL);
    if (!user_valid_for_insert(u))
        return 0;

    db_user dbu = bdb_user_database(srv);

    DB_TXN *txn = bdb_server_begin(srv);
    if (txn == NULL)
        goto err;
    
    // Get a sequence number 
    db_seq_t dbid;
    ret = dbu->seq->get(dbu->seq, NULL /* txn */, 1, &dbid, 0);
    if (ret)
        goto err;

    // Assign the sequence number as the new PK
    user_set_id(u, (dbid_t)dbid);
    user_set_created(u, now());

    // Write the record to the database (big moment)
    DBT key, data;
    memset(&key, 0, sizeof key);
    memset(&data, 0, sizeof data);
    key.data = &dbid;
    key.size = sizeof dbid;

    data.data = u;
    data.size = user_size();
    ret = dbu->dbs[DB_USER_USER].dbp->put(dbu->dbs[DB_USER_USER].dbp, txn, &key, &data, DB_NOOVERWRITE);
    if (ret)
        goto err;

    bdb_server_commit(srv, txn);
    return (dbid_t)dbid;

err:
    fprintf(stderr, "Meh, hit err with ret == %d: %s\n", ret, db_strerror(ret));
    bdb_server_rollback(srv, txn);
    return 0;
}

