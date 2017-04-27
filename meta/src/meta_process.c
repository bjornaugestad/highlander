/*
 * libhighlander - A HTTP and TCP server-side library
 * Copyright (C) 2013 B. Augestad, bjorn@augestad.online
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <meta_common.h>
#include <cstring.h>
#include <meta_misc.h>

#include <meta_process.h>

/*
 * This small struct stores objects we handle.
 */
struct srv {
    void *object;
    status_t (*do_func)(void *object);
    status_t (*undo_func)(void *object);
    status_t (*run_func)(void *object);
    status_t (*shutdown_func)(void *object);

    /* We must wait for each started object to finish
     * so that the main process can e.g. free the object
     * after process_wait_for_shutdown() returns.
     * Here's the thread id for the object started.
     */
    pthread_t tid;

    /* Store the exit code from the run_func() function here. */
    intptr_t exitcode;
};

/*
 * Max number of objects a process can start.
 * Each object uses (on x86) 7*4 bytes, so this is
 * very low overhead.
 */
#define MAX_OBJECTS 200


/*
 * Implementation of our process ADT.
 */
struct process_tag {
    cstring appname;
    cstring rootdir;
    cstring username;

    struct srv objects[MAX_OBJECTS];
    size_t objects_used;

    int shutting_down;

    /* The shutdown thread */
    pthread_t sdt;
};

process process_new(const char *appname)
{
    process new;

    if ((new = calloc(1, sizeof *new)) == NULL)
        ;
    else if ((new->rootdir = cstring_new()) == NULL
    || (new->appname = cstring_new()) == NULL
    || (new->username = cstring_new()) == NULL
    || !cstring_set(new->appname, appname)) {
        cstring_free(new->appname);
        cstring_free(new->username);
        cstring_free(new->rootdir);
        free(new);
        new = NULL;
    }
    else {
        new->objects_used = 0;
        new->shutting_down = 0;
    }

    return new;
}

void process_free(process this)
{
    if (this != NULL) {
        cstring_free(this->appname);
        cstring_free(this->username);
        cstring_free(this->rootdir);
        free(this);
    }
}

status_t process_set_rootdir(process this, const char *path)
{
    assert(this != NULL);
    assert(path != NULL);

    return cstring_set(this->rootdir, path);
}

status_t process_set_username(process this, const char *username)
{
    assert(this != NULL);
    assert(username != NULL);

    return cstring_set(this->username, username);
}

status_t process_add_object_to_start(
    process this,
    void *object,
    status_t do_func(void *),
    status_t undo_func(void *),
    status_t run_func(void *),
    status_t shutdown_func(void *))
{
    assert(this != NULL);
    assert(object != NULL);

    /* We need either both the do/undo functions or none */
    assert(do_func != NULL || undo_func == NULL);
    assert(undo_func != NULL || do_func == NULL);

    assert(run_func != NULL);
    assert(shutdown_func != NULL);

    if (this->objects_used == MAX_OBJECTS) {
        errno = ENOSPC;
        return failure;
    }

    this->objects[this->objects_used].object = object;
    this->objects[this->objects_used].do_func = do_func;
    this->objects[this->objects_used].undo_func = undo_func;
    this->objects[this->objects_used].run_func = run_func;
    this->objects[this->objects_used].shutdown_func = shutdown_func;
    this->objects[this->objects_used].exitcode = -1;
    this->objects_used++;
    return success;
}

/*
 * We ignore SIGPIPE for the entire process.
 * This way write() will return -1 and errno will be EPIPE when the
 * client has disconnected and we try write to the socket.
 * This function returns 1 on success, else 0.
 */
static int set_signals_to_block(void)
{
    sigset_t block;
    int xsuccess = 0;

    errno = 0;
    if (SIG_ERR == signal(SIGPIPE, SIG_IGN))
        debug("%s: signal failed\n", __func__);
    else if (-1 == sigemptyset(&block))
        debug("%s: sigempty failed\n", __func__);
    else if (-1 == sigaddset(&block, SIGTERM))
        debug("%s: sigaddset failed\n", __func__);
    else if (pthread_sigmask(SIG_BLOCK, &block, NULL))
        debug("%s: pthread_sigmask failed\n", __func__);
    else
        xsuccess = 1;

    if (!xsuccess && errno == 0)
        errno = EINVAL;

    return xsuccess;
}

int process_shutting_down(process this)
{
    return this->shutting_down;
}

/*
 * Write the pid to /var/run/appname.pid
 * Return 1 on success, 0 on error.
 */
static int write_pid(process this, pid_t pid)
{
    FILE *f;
    char filename[1024];

    assert(this != NULL);

    /* NOTE: Try to find a portable way of identifying the proper
     * directory to write this file in. AIX 4.3.3 has no /var/run.
     */
    snprintf(filename, sizeof filename, "/var/run/%s.pid", c_str(this->appname));

    if ((f = fopen(filename, "w")) == NULL)
        return 0;

    if (fprintf(f, "%lu", (unsigned long)pid) <= 0) {
        fclose(f);
        return 0;
    }

    if (fclose(f))
        return 0;

    return 1;
}

static void *shutdown_thread(void *arg)
{
    sigset_t catch;
    int caught;
    process proc;
    pid_t my_pid;
    size_t i;

    proc = arg;

    /*
     * Since this is the thread to receive the signal,
     * we save this threads pid. This is important under Linux,
     * as each thread has its own pid.
     */
    my_pid = getpid();
    if (!write_pid(proc, my_pid)) {
        warning("Unable to write pid %lu to the pid file.",
            (unsigned long)my_pid);
    }

    sigemptyset(&catch);
    sigaddset(&catch, SIGTERM);

    /* Wait, wait, wait for SIGTERM */
    sigwait(&catch, &caught);
    proc->shutting_down = 1;

    /* Shut down all objects we handle */
    for (i = 0; i < proc->objects_used; i++) {
        struct srv* s = &proc->objects[i];
        s->shutdown_func(s->object);
    }

    return NULL;
}

static status_t handle_shutdown(process this)
{
    int error;

    /* Block the signals we handle before creating threads */
    if (!set_signals_to_block()) {
        debug("Could not block signals\n");
        return failure;
    }

    /* start the shutdown thread */
    if ((error = pthread_create(&this->sdt, NULL, shutdown_thread, this))) {
        debug("Could not create debug thread\n");
        errno = error;
        return failure;
    }

    return success;
}

static void *launcher(void *args)
{
    status_t exitcode;
    struct srv* s = args;
    exitcode = s->run_func(s->object);
    return (void*)(intptr_t)exitcode;
}

/*
 * Handle the thread creation needed to create a thread
 * which starts the actual server.
 */
static int start_one_object(struct srv* s)
{
    assert(s != NULL);

    return pthread_create(&s->tid, NULL, launcher, s);
}

/* Calls the undo() function for all objects for which the do()
 * function has been called. Used if we encounter
 * an error during start up and want to undo the startup for
 * objects already started.
 * The pointer to the failed object, failed, may be NULL. This
 * value means that the undo() function should be called for all objects.
 */
static void undo(process this, struct srv* failed)
{
    size_t i;
    struct srv *s;

    for (i = 0; i < this->objects_used; i++) {
        s = &this->objects[i];
        if (s == failed)
            return;

        if (s->undo_func != NULL)
            s->undo_func(s->object);
    }
}

static void shutdown_started_objects(process this, struct srv* failed)
{
    size_t i;
    struct srv *s;

    for (i = 0; i < this->objects_used; i++) {
        s = &this->objects[i];
        if (s == failed)
            return;

        /* Shut down the object and wait to get the exitcode */
        s->shutdown_func(s->object);
        assert(s->tid);
        pthread_join(s->tid, NULL);
    }
}

/*
 * If we fail after the shutdown thread has started, we have to kill it.
 * That's done here.
 */
static void shutdown_shutdown(process this)
{
    pthread_kill(this->sdt, SIGTERM);
}


status_t process_start(process this, int fork_and_close)
{
    size_t i;
    struct srv *s;
    int error;

    assert(this != NULL);

    if (fork_and_close) {
        pid_t pid;

        if ((pid = fork()) == -1)
            return failure;
        else if (pid != 0)
            exit(EXIT_SUCCESS);

        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
    }

    for (i = 0; i < this->objects_used; i++) {
        s = &this->objects[i];
        if (s->do_func != NULL && !s->do_func(s->object)) {
            undo(this, s);
            return failure;
        }
    }

    /* Start shutdown thread before we do setuid() or chroot() to
     * be able to write to /var/run. */
    if (!handle_shutdown(this)) {
        undo(this, NULL);
        return failure;
    }

    /* Set current directory and user id if supplied by the caller */
    if (cstring_length(this->username) > 0) {
        struct passwd* pw;
        errno = 0;
        if ((pw = getpwnam(c_str(this->username))) == NULL) {
            shutdown_shutdown(this);
            undo(this, NULL);

            /* Hmm, either the user didn't exist in /etc/passwd
             * or we didn't have permission to read it or we ran
             * out of memory. The Linux man page doesn't mention
             * any other errno value than ENOMEM, which is why we
             * set and test errno before return.
             */
            if (errno == 0)
                errno = ENOENT;

            debug("Could not get username. getpwnam() failed\n");
            return failure;
        }

        /* We must chroot before setuid() to be allowed to chroot. */
        if (cstring_length(this->rootdir) > 0) {
            if (chdir(c_str(this->rootdir)) || chroot(c_str(this->rootdir))) {
                /* Hmm, unable to change directory. */
                shutdown_shutdown(this);
                undo(this, NULL);
                debug("Could not change root directory\n");
                return failure;
            }
        }

        if (setuid(pw->pw_uid)) {
            /* Oops, unable to change user id. This is serious
             * since the process may continue running as e.g. root.
             * The safest thing to do is therefore to stop
             * the process.
             */
            undo(this, NULL);
            shutdown_shutdown(this);
            debug("Could not set uid\n");
            return failure;
        }
    }
    else {
        /* Don't chroot() twice */
        if (cstring_length(this->rootdir) > 0) {
            if (chdir(c_str(this->rootdir)) || chroot(c_str(this->rootdir))) {
                /* Hmm, unable to change directory. */
                undo(this, NULL);
                shutdown_shutdown(this);
                return failure;
            }
        }
    }

    /* Now start the server objects */
    for (i = 0; i < this->objects_used; i++) {
        s = &this->objects[i];
        if ((error = start_one_object(s))) {
            /* Hmm, we failed to start one object. Do not
             * start the rest of the objects. Call the shutdown
             * function for each object to tell it to stop running.
             */
            shutdown_started_objects(this, s);
            shutdown_shutdown(this);
            return failure;
        }
    }

    /* Success. */
    return success;
}

status_t process_wait_for_shutdown(process this)
{
    size_t i;
    int error;

    assert(this != NULL);
    assert(this->sdt);
    if ((error = pthread_join(this->sdt, NULL))) {
        errno = error;
        return failure;
    }

    /* Now that the shutdown thread has exited, it
     * is time to wait for the started objects.
     */
    for (i = 0; i < this->objects_used; i++) {
        struct srv* srv = &this->objects[i];
        void *pfoo = &srv->exitcode;
        assert(srv->tid);
        if ((error = pthread_join(srv->tid, &pfoo))) {
            errno = error;
            return failure;
        }
    }

    return success;
}

int process_get_exitcode(process this, void *object)
{
    size_t i;

    assert(this != NULL);
    assert(object != NULL);

    for (i = 0; i < this->objects_used; i++) {
        struct srv* s = &this->objects[i];
        if (s->object == object)
            return s->exitcode;
    }

    return -1;
}
