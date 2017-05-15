#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include <metal.h>
#include <metal_limits.h>

#include "meta_fifo.h"

// What we store in the message queue. Trouble is that we do not
// want to allocate and free each message. That'd be too slow.
// Hmm, maybe a generic FIFO is the wrong alternative here?
//
// Maybe the task should use a fixed size array to store its messages?
// OTOH, it's nice to have the FIFO's signal/wait/lock semantics.
// Hmm, how can we get both? An in-place ctor? A resource pool?
struct message {
    tid_t sender;
    msgid_t msg;
    msgarg_t arg1, arg2;
};

// The task data structure. Each running task has one.
// The task struct holds the message queue. The queue is a FIFO queue,
// so we can utilize meta_fifo.
//
struct task_tag {
    tid_t tid;

    char name[METAL_TASKNAME_MAX + 1];
    int instance;

    taskfn fn;

    pthread_t threadid;

    fifo q;

    // Who subscribes to this task's published messages?
    // Keep the array entries left-shifted, so if someone
    // unsubscribes, we shift other entries to the left if needed.
    tid_t subscribers[METAL_MAXSUBSCRIBERS];
    pthread_rwlock_t sublock;
};

status_t task_subscriber_add(task p, tid_t tid)
{
    int err;
    size_t i;

    assert(p != NULL);

    if ((err = pthread_rwlock_wrlock(&p->sublock)) != 0)
        return fail(err);

    for (i = 0; i < sizeof p->subscribers / sizeof *p->subscribers; i++) {
        if (p->subscribers[i] == tid) {
            pthread_rwlock_unlock(&p->sublock);
            return fail(EINVAL);
        }
        else if (p->subscribers[i] == 0) {
            p->subscribers[i] = tid;
            pthread_rwlock_unlock(&p->sublock);
            return success;
        }
    }

    // No room
    pthread_rwlock_unlock(&p->sublock);
    return fail(ENOSPC);
}

// Remove the tid, if found in the subscribers array.
// No biggie if the tid isn't in the array.
status_t task_subscriber_remove(task p, tid_t tid)
{
    int err;
    size_t i, n;
    bool shifting = false;

    assert(p != NULL);

    if ((err = pthread_rwlock_wrlock(&p->sublock)) != 0)
        return fail(err);

    n = sizeof p->subscribers / sizeof *p->subscribers;
    for (i = 0; i < n; i++) {
        if (p->subscribers[i] == 0)
            break;

        if (!shifting && p->subscribers[i] == tid)
            shifting = true;

        if (shifting && i + 1 < n)
            p->subscribers[i] = p->subscribers[i + 1];
    }

    pthread_rwlock_unlock(&p->sublock);
    return success;
}

status_t message_publish(msgid_t msg, msgarg_t arg1, msgarg_t arg2)
{
    task p;
    size_t i;
    int err;

    p = self();

    if ((err = pthread_rwlock_rdlock(&p->sublock)) != 0)
        return fail(err);

    for (i = 0; i < sizeof p->subscribers / sizeof *p->subscribers; i++) {
        if (p->subscribers[i] == 0)
            break;
        if (!message_send(p->tid, p->subscribers[i], msg, arg1, arg2))
            puts("Meh");
    }

    pthread_rwlock_unlock(&p->sublock);
    return success;
}


task task_new(void)
{
    task p;
    int err;

    if ((p = calloc(1, sizeof *p)) == NULL)
        return NULL;

    if ((err = pthread_rwlock_init(&p->sublock, NULL)) != 0) {
        free(p);
        return NULL;
    }

    if ((p->q = fifo_new(METAL_TASK_QUEUE_SIZE)) == NULL) {
        free(p);
        return NULL;
    }

    p->name[0] = '\0';
    p->tid = 0; // system tid. Hmm... -1 is not allowed.

    return p;
}

status_t task_init(task p, const char *name, int instance, taskfn fn, tid_t tid)
{
    assert(p != NULL);
    assert(name != NULL);
    assert(fn != 0);

    p->threadid = 0;
    task_set_tid(p, tid);
    task_set_fn(p, fn);
    task_set_name(p, name);
    task_set_instance(p, instance);

    return success;
}

void task_free(task p)
{
    if (p != NULL) {
        fifo_free(p->q, free);
        pthread_rwlock_destroy(&p->sublock);
        free(p);
    }
}

static void *wrap_pthread_fn(void *vtask)
{
    task p;

    assert(vtask != NULL);

    p = vtask;

    assert(p->fn != 0);
    p->fn();
    return NULL;
}

status_t task_start(task p)
{
    int err;
    pthread_attr_t attr;

    assert(p != NULL);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    err = pthread_create(&p->threadid, &attr, wrap_pthread_fn, p);
    pthread_attr_destroy(&attr);

    if (err == 0)
        return success;

    return fail(err);
}

status_t task_message_add(task p, tid_t sender, msgid_t msg, msgarg_t arg1, msgarg_t arg2)
{
    struct message *pm;
    status_t rc;

    assert(p != NULL);

    // This is silly because it's slow. TODO: Figure out a way
    // to allocate meta_fifos inplace.
    if ((pm = malloc(sizeof *pm)) == NULL)
        return failure;

    pm->sender = sender;
    pm->msg = msg;
    pm->arg1 = arg1;
    pm->arg2 = arg2;

    rc = fifo_write_signal(p->q, pm);
    if (!rc)
        free(pm);

    return rc;
}

// Grab a lock on the FIFO. Then do a condwait until we get a message.
//
status_t message_get(tid_t *sender, msgid_t *msg, msgarg_t *arg1, msgarg_t *arg2)
{
    struct message *pm;

    task p = self();

    // First lock the queue to see if there's a message available.
    if (!fifo_lock(p->q))
        return false;

    if (fifo_nelem(p->q) == 0) {
        if (!fifo_unlock(p->q))
            return false;

        if (!fifo_wait_cond(p->q))
            return false;
    }

    // We now hold a lock.
    assert(fifo_nelem(p->q) > 0);

    if ((pm = fifo_get(p->q)) == NULL)
        die("%s(): Internal error\n", __func__);

    *sender = pm->sender;
    *msg = pm->msg;
    *arg1 = pm->arg1;
    *arg2 = pm->arg2;

    fifo_unlock(p->q);
    free(pm);
    return success;
}


void task_set_name(task p, const char *name)
{
    assert(p != NULL);
    assert(strlen(name) < sizeof p->name);

    strcpy(p->name, name);
}

void task_set_tid(task p, tid_t tid)
{
    assert(p != NULL);
    p->tid = tid;
}

void task_set_fn(task p, taskfn fn)
{
    assert(p != NULL);
    p->fn = fn;
}

const char* task_name(task p)
{
    assert(p != NULL);
    return p->name;
}

tid_t task_tid(task p)
{
    assert(p != NULL);
    return p->tid;
}

pthread_t task_threadid(task p)
{
    assert(p != NULL);
    return p->threadid;
}

int task_instance(task p)
{
    assert(p != NULL);
    return p->instance;
}

void task_set_instance(task p, int instance)
{
    assert(p != NULL);
    p->instance = instance;
}
