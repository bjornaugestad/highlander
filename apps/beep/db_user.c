#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <bdb_server.h>
#include <db_user.h>

int get_user_name(DB *dbnamep, const DBT *pkey, const DBT *pdata, DBT *namep_key)
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

int get_user_nick(DB *dbnickp, const DBT *pkey, const DBT *pdata, DBT *nickp_key)
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

int get_user_email(DB *dbemailp, const DBT *pkey, const DBT *pdata, DBT *emailp_key)
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

    DB *dbp = bdb_server_get_db(srv, DB_USER_USER);

    DB_TXN *txn = bdb_server_begin(srv);
    if (txn == NULL)
        goto err;
    
    // Get a sequence number 
    DB_SEQUENCE *seq = bdb_server_get_sequence(srv, DB_USER_SEQUENCE);
    db_seq_t dbid;
    ret = seq->get(seq, NULL /* txn */, 1, &dbid, 0);
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
    ret = dbp->put(dbp, txn, &key, &data, DB_NOOVERWRITE);
    if (ret)
        goto err;

    bdb_server_commit(srv, txn);
    return (dbid_t)dbid;

err:
    fprintf(stderr, "Meh, hit err with ret == %d: %s\n", ret, db_strerror(ret));
    bdb_server_rollback(srv, txn);
    return 0;
}

