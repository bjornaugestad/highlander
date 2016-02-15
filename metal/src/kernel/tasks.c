/*
 * A task is a central concept in metal. It is a POSIX thread
 * with an associated message queue.
 *
 * Tasks publish messages, and other tasks can subscribe to 
 * these messages.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <meta_fifo.h>

#include "metal.h"
#include "metal_limits.h"

static tid_t tidcounter = 1; // 0 is reserved.

// Note: this is overkill and silly too, since the number of running
// tasks is fixed to METAL_MAXTASKS. IOW, tid cannot be >= that value.
// We don't need tid_get() as long as we use an array. If we swich to
// another data structure, then things change.
static tid_t tid_get(void)
{
    tid_t next;
    static pthread_mutex_t tidlock = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&tidlock);
    next = tidcounter++;
    if (next == 0) {
        // We wrapped, but 0 is reserved. Skip it.
        next++;
        tidcounter++;
    }
    pthread_mutex_unlock(&tidlock);
    return next;
}

// Right, we need a big, big lock to protect the main task struct, so we can
// do lookups and inserts (and deletions) in a thread safe way. We need a list
// of tasks, or an array of tasks. Arrays are faster, but can't grow beyond
// some limit. We go for an array since it's faster to look up stuff in arrays
// and we mostly do lookups, e.g. when sending messages.
static task tasks[METAL_MAXTASKS];

// This rwlock is used when accessing the tasks array. We need to lock both
// read access and write access, and escalate lock from read to write in
// some cases. We try to stay read-locked as much as possible, to allow
// other threads to exchange messages when we e.g. scan the array.
static pthread_rwlock_t taskslock = PTHREAD_RWLOCK_INITIALIZER;

static task find_task(const char *name, int instance)
{
    size_t i, n = sizeof tasks / sizeof *tasks;

    for (i = 0; i < n; i++) {
        if (tasks[i] != NULL 
        && strcmp(task_name(tasks[i]), name) == 0
        && task_instance(tasks[i]) == instance)
            return tasks[i];
    }

    return NULL;
}

static task find_task_by_tid(tid_t tid)
{
    size_t i, n = sizeof tasks / sizeof *tasks;

    for (i = 0; i < n; i++) {
        if (tasks[i] != NULL && task_tid(tasks[i]) == tid)
            return tasks[i];
    }

    return NULL;
}

task self(void)
{
    return find_task_by_tid(self_tid());
}

static void clear_task_entry(task p)
{
    size_t i, n = sizeof tasks / sizeof *tasks;

    for (i = 0; i < n; i++) {
        if (tasks[i] == p) {
            tasks[i] = NULL;
            return;
        }
    }

    assert(0 && "Shouldn't be here. p didn't refer to a task.");
}


// Return index to free task, or 0 if no free tasks exist.
// 0 is system task, and that one is never free.
static size_t find_free_task(void)
{
    size_t i, n = sizeof tasks / sizeof *tasks;

    for (i = 0; i < n; i++) {
        if (tasks[i] == NULL)
            return i;
    }

    return 0;
}

tid_t self_tid(void)
{
    size_t i, n;
    pthread_t ptid;
    int err;
    tid_t tid;
    
    if ((err = pthread_rwlock_rdlock(&taskslock)) != 0)
        die("Internal error.");

    ptid = pthread_self();
    n = sizeof tasks / sizeof *tasks;
    for (i = 0; i < n; i++) {
        if (tasks[i] != NULL) {
            pthread_t t = task_threadid(tasks[i]);

            if (pthread_equal(t, ptid))
                goto found;
        }
    }

    die("Internal error: All threads must have a tid.\n");

found:
    tid = task_tid(tasks[i]);
    pthread_rwlock_unlock(&taskslock);
    return tid;
}

status_t publish(msgid_t msg, msgarg_t arg1, msgarg_t arg2) 
{
    int err;
    status_t rc;

    if ((err = pthread_rwlock_rdlock(&taskslock)) != 0)
        die("Internal error.");
    
    rc = message_publish(msg, arg1, arg2);

    pthread_rwlock_unlock(&taskslock);
    return rc;
}

status_t metal_task_new(tid_t *tid, const char *name, int instance, taskfn fn)
{
    int err;
    size_t i;

    assert(name != NULL);
    assert(strlen(name) <= METAL_TASKNAME_MAX);

    // Get a new slot from the task array. This involves locking the array,
    // checking for duplicate names, checking for free slots, and finally
    // marking one slot as used.
    if ((err = pthread_rwlock_wrlock(&taskslock)) != 0)
        return fail(err);

    if (find_task(name, instance) != NULL) {
        pthread_rwlock_unlock(&taskslock);
        return fail(EINVAL);
    }

    if ((i = find_free_task()) == 0) {
        pthread_rwlock_unlock(&taskslock);
        return fail(ENOSPC);
    }

    if ((tasks[i] = task_new()) == NULL) {
        pthread_rwlock_unlock(&taskslock);
        return failure;
    }

    *tid = tid_get();
    if (!task_init(tasks[i], name, instance, fn, *tid)) {
        task_free(tasks[i]);
        tasks[i] = NULL;
        pthread_rwlock_unlock(&taskslock);
        return failure;
    }

    pthread_rwlock_unlock(&taskslock);
    return success;
}

// task_new() allocates resources which must be freed somehow.
// At the same time, the task object lives in an array, so we
// cannot just free it. Hence we reset it.
status_t metal_task_stop(tid_t tid)
{
    task p;
    size_t i;

    assert(tid > 0); // Can't reset the system task

    // TODO: Stop the task first. We need the message queue to be able to do that.
    if (!message_send(0, tid, MM_EXIT, 0, 0))
        return failure;

    // Tasks need time to process the MM_EXIT message.
    usleep(5000);

    // Remember that writelocks blocks messages
    pthread_rwlock_wrlock(&taskslock);

    if ((p = find_task_by_tid(tid)) == NULL) {
        pthread_rwlock_unlock(&taskslock);
        return fail(ENOENT);
    }

    for (i = 0; i < sizeof tasks / sizeof *tasks; i++) {
        if (tasks[i] != NULL && !task_subscriber_remove(tasks[i], tid))
            die("Unable to remove subscription");
    }

    usleep(500);
    task_free(p);
    clear_task_entry(p);
    pthread_rwlock_unlock(&taskslock);
    return success;
}

status_t metal_task_start(tid_t tid)
{
    task p;
    status_t rc;
    int err;

    if ((err = pthread_rwlock_rdlock(&taskslock)) != 0)
        return fail(err);

    if ((p = find_task_by_tid(tid)) == NULL) {
        pthread_rwlock_unlock(&taskslock);
        return fail(ENOENT);
    }

    rc = task_start(p);
    pthread_rwlock_unlock(&taskslock);
    return rc;
}

void systemtask(void)
{
}

status_t metal_init(int flags)
{
    memset(tasks, 0, sizeof tasks);

    if ((tasks[0] = task_new()) == NULL)
        return failure;

    if (!task_init(tasks[0], "system", 0, systemtask, 0)) {
        task_free(tasks[0]);
        tasks[0] = NULL;
        return failure;
    }

    (void)flags;
    return success;
}

status_t metal_exit(void)
{
    size_t i, n;

    n = sizeof tasks / sizeof *tasks;
    for (i = 0; i < n; i++) {
        //TODO: We want to do more shutdown than just freeing task mem.
        task_free(tasks[i]);
    }

    return success;
}

status_t metal_subscribe(tid_t publisher, tid_t subscriber)
{
    task p;

    assert(publisher != 0);
    assert(subscriber != 0);

    if ((p = find_task_by_tid(publisher)) == NULL)
        return fail(ENOENT);

    // Check that subscriber task exists
    assert(find_task_by_tid(subscriber) != NULL);

    return task_subscriber_add(p, subscriber);
}

// So we want to send a message to a specific task. We can do that.
// 1. Check if the task exists or not.
// 2. Add message to the tasks' queue.
// 3. Profit. :)
//
// Locking: It's sufficient to read-lock the tasks array.
status_t message_send(tid_t sender, tid_t dest, msgid_t msg, msgarg_t arg1, msgarg_t arg2) 
{
    task p;
    int err;

    if ((err = pthread_rwlock_rdlock(&taskslock)) != 0)
        return fail(err);

    if ((p = find_task_by_tid(dest)) == NULL) {
        errno = ENOENT;
        goto error;
    }

    if (!task_message_add(p, sender, msg, arg1, arg2))
        goto error;

    pthread_rwlock_unlock(&taskslock);
    return success;

error:
    fprintf(stderr, "meh in %s\n", __func__);
    pthread_rwlock_unlock(&taskslock);
    return failure;
    
}



#ifdef TASKS_CHECK
#include <stdio.h>
#include <unistd.h>

static void foofn(void)
{
    puts(__func__);
}

int main(void)
{
    size_t i, n;

    if (!metal_init(0))
        die("Meh");

    for (i = 0, n = 100; i < n; i++) {
        tid_t tid;
        if (!metal_task_new(&tid, "foo", 0, foofn))
            die("Meh 3");

        if (!metal_task_start(tid))
            die("task_start\n");

        usleep(1000);
        if (!metal_task_stop(tid))
            die("task_stop\n");
    }
        

    if (!metal_exit())
        die("Meh 2");

    return 0;
}
#endif

