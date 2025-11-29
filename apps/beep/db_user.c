#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <bdb_server.h>
#include <db_user.h>

// Lots of databases. One primary, dbp, three secondary ones for unique constraints,
// and finally a place for the user sequence to be stored.
//
struct db_user {
    DB *dbp, *dbnamep, *dbnickp, *dbemailp, *dbseqp;
    DB_SEQUENCE *seq;
    DBT seqkey;
};

db_user db_user_new(void)
{
    db_user new = calloc(1, sizeof *new);
    if (new == NULL)
        return NULL;

    return new;
}

void db_user_free(db_user u)
{
    if (u == NULL)
        return;

    // Now for some hard questions: We want to keep the new->open->close->free idiom,
    // so what if close hasn't been called? As always, assert() FTW 
    //
    assert(u->dbp == NULL);
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
    if ((ret = db_create(&u->dbp, envp, 0))
    ||  (ret = db_create(&u->dbnamep, envp, 0))
    ||  (ret = db_create(&u->dbnickp, envp, 0))
    ||  (ret = db_create(&u->dbemailp, envp, 0))
    ||  (ret = db_create(&u->dbseqp, envp, 0))) {
        fprintf(stderr, "Could not create db: %s\n", db_strerror(ret));
        return failure;
    }

    uint32_t flags = DB_CREATE | DB_THREAD;
    ret = u->dbp->open(u->dbp, NULL, "users.db", NULL, DB_BTREE, flags, 0);
    ret = !ret && u->dbnamep->open(u->dbnamep, NULL, "users.db", NULL, DB_BTREE, flags, 0);
    ret = !ret && u->dbnickp->open(u->dbnickp, NULL, "users.db", NULL, DB_BTREE, flags, 0);
    ret = !ret && u->dbemailp->open(u->dbemailp, NULL, "users.db", NULL, DB_BTREE, flags, 0);
    ret = !ret && u->dbseqp->open(u->dbseqp, NULL, "users.db", NULL, DB_BTREE, flags, 0);
    if (ret != 0) {
        fprintf(stderr, "Could not open db: %s\n", db_strerror(ret));
        return failure;
    }

    // Associate primary db with secondary ones
    u->dbp->associate(u->dbp, NULL, u->dbnamep,  get_user_name,  0);
    u->dbp->associate(u->dbp, NULL, u->dbnickp,  get_user_nick,  0);
    u->dbp->associate(u->dbp, NULL, u->dbemailp, get_user_email, 0);

    // Deal with the sequence too
    ret = db_sequence_create(&u->seq, u->dbseqp, 0);
    if (ret != 0)
        goto monster_err;

    char seqname[] = "user_sequence"; // NOTE: lifespan looks fishy. Move to ADT?
    memset(&u->seqkey, 0, sizeof u->seqkey);
    u->seqkey.data = seqname;
    u->seqkey.size = sizeof seqname;
    ret = u->seq->open(u->seq, NULL, &u->seqkey, flags);
    if (ret != 0)
        goto monster_err;


    return success;

monster_err: // A hell of a lot to clean up
    fprintf(stderr, "Meh, hit monster err with ret == %d: %s\n", ret, db_strerror(ret));
    return failure;
}

status_t db_user_close(db_user u)
{
    assert(u != NULL);
    assert(u->dbp != NULL);

    // Tear it all down, in reverse order.
    u->seq->close(u->seq, 0);
    u->dbseqp->close(u->dbseqp, 0);
    u->dbnamep->close(u->dbnamep, 0);
    u->dbnickp->close(u->dbnickp, 0);
    u->dbemailp->close(u->dbemailp, 0);
    u->dbp->close(u->dbp, 0);
    u->dbp = NULL;

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

    return true;
}


// Notes from the western front: Bdb is harder than RMDBS
// We want to insert one row in the users table
// We need a DB_SEQUENCE for the users table
// We need two secondary databases for the two unique columns(name, nick), perhaps even three(email)
// We need a number sorter function for the id field.
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
dbid_t bdb_user_add(bdb_server srv, User u)
{
    assert(u != NULL);
    if (!user_valid_for_insert(u))
        return 0;

    db_user dbu = bdb_user_database(srv);
    
    // Get a sequence number 
    db_seq_t dbid;
    int ret = dbu->seq->get(dbu->seq, NULL, 1, &dbid, 0);
    if (ret)
        goto err;

    // Assign the sequence number as the new PK
    user_set_id(u, (dbid_t)dbid);

    // Write the record to the database (big moment)
    DBT key, data;
    memset(&key, 0, sizeof key);
    memset(&data, 0, sizeof data);
    key.data = &dbid;
    key.size = sizeof dbid;

    data.data = u;
    data.size = user_size();
    ret = dbu->dbp->put(dbu->dbp, NULL, &key, &data, DB_NOOVERWRITE);
    if (ret)
        goto err;

    return (dbid_t)dbid;

err:
    fprintf(stderr, "Meh, hit err with ret == %d: %s\n", ret, db_strerror(ret));
    return 0;
    
}

