#include <assert.h>
#include <stdlib.h>

#include <db_user.h>

struct db_user {
    DB *dbp;
};

db_user db_user_new(void)
{
    db_user new = malloc(sizeof *new);
    if (new == NULL)
        return NULL;

    new->dbp = NULL;
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

status_t db_user_open(db_user u, DB_ENV *envp)
{
    assert(u != NULL);

    int ret = db_create(&u->dbp, envp, 0);
    if (ret != 0) {
        fprintf(stderr, "Could not create db: %s\n", db_strerror(ret));
        return failure;
    }

    ret = u->dbp->open(u->dbp, NULL, "users.db", NULL, DB_BTREE, DB_CREATE | DB_THREAD, 0);
    if (ret != 0) {
        fprintf(stderr, "Could not open db: %s\n", db_strerror(ret));
        return failure;
    }

    return success;
}

status_t db_user_close(db_user u)
{
    assert(u != NULL);
    assert(u->dbp != NULL);
    u->dbp->close(u->dbp, 0);
    u->dbp = NULL;

    return success;
}
