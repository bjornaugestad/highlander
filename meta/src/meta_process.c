/*
 * Copyright (C) 2001-2022 Bjorn Augestad, bjorn.augestad@gmail.com
 * All Rights Reserved. See COPYING for license details
 */

#define _GNU_SOURCE
#include <unistd.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdatomic.h>

#include <meta_common.h>
#include <cstring.h>
#include <meta_misc.h>

#include <meta_process.h>

// This small struct stores services we handle. A service is basically
// a thread doing whatever. It has a this pointer, a tid, and four
// functions: do/undo is used pre-start, run is to start running, and
// shutdown is for shutting it down.
// The idea is to be able to plug in different types of services
// into a common process-object and manage them all at the same time.
// For example: A process may run two http(s) services and a management
// console as three different main threads.
struct service {
    void *object;
    status_t (*do_func)(void *object);
    status_t (*undo_func)(void *object);
    status_t (*run_func)(void *object);
    status_t (*shutdown_func)(void *object);

    // We must wait for each started object to finish so that the main process
    // can e.g. free the object after process_wait_for_shutdown() returns.
    // Here's the thread id for the object started.
    pthread_t tid;

    // Store the exit code from the run_func() function here.
    intptr_t exitcode;
};

// Max number of services a process can start
#define MAX_SERVICES 200


// Implementation of our process ADT.
struct process_tag {
    cstring appname;
    cstring rootdir;
    cstring username;

    struct service services[MAX_SERVICES];
    size_t nservices;

    _Atomic bool shutting_down;

    /* The shutdown thread */
    pthread_t sdt;
};

process process_new(const char *appname)
{
    process new;

    assert(appname != NULL);
    assert(strlen(appname) > 0);

    if ((new = calloc(1, sizeof *new)) == NULL)
        goto memerr;

    if ((new->rootdir = cstring_new()) == NULL
    || (new->appname = cstring_dup(appname)) == NULL
    || (new->username = cstring_new()) == NULL)
        goto memerr;

    new->nservices = 0;
    atomic_store_explicit(&new->shutting_down, false, memory_order_relaxed);
    return new;

memerr:
    process_free(new);
    return NULL;
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

    // We need either both the do/undo functions or none
    assert(do_func != NULL || undo_func == NULL);
    assert(undo_func != NULL || do_func == NULL);

    assert(run_func != NULL);
    assert(shutdown_func != NULL);

    if (this->nservices == MAX_SERVICES)
        return fail(ENOSPC);

    this->services[this->nservices].object = object;
    this->services[this->nservices].do_func = do_func;
    this->services[this->nservices].undo_func = undo_func;
    this->services[this->nservices].run_func = run_func;
    this->services[this->nservices].shutdown_func = shutdown_func;
    this->services[this->nservices].exitcode = -1;
    this->nservices++;

    return success;
}

// We ignore SIGPIPE for the entire process.
// This way write() will return -1 and errno will be EPIPE when the
// client has disconnected and we try write to the socket.
static status_t set_signals_to_block(void)
{
    sigset_t block;
    status_t rc = failure;

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
        rc = success;

    if (!rc && errno == 0)
        errno = EINVAL;

    return rc;
}

int process_shutting_down(process this)
{
    return atomic_load_explicit(&this->shutting_down, memory_order_relaxed);
}

// Write the pid to /var/run/appname.pid
// 20251008: Fuck Lennart. /var/run -> /run and we don't have perms there.
// pid-files are supposed to go to /run/appname/%d.pid, but appname is
// wiped after boot and we need root to create the directory. IOW, we're fucked.
//
// Unit tests need the pid to be able to kill the process. If the pid is part
// of the file's name, we have no deterministic way of finding the pid, so
// we go with process.name + pid in current directory if we can't write it to
// /run/highlander/name+pid.
static status_t write_pid(process this, pid_t pid)
{
    FILE *f;
    char filename[1024];

    assert(this != NULL);

    snprintf(filename, sizeof filename, "/var/run/highlander/%s.pid", c_str(this->appname));

    if ((f = fopen(filename, "w")) == NULL) {
        // Open local version
        snprintf(filename, sizeof filename, "./%s.pid", c_str(this->appname));

        if ((f = fopen(filename, "w")) == NULL)
            return failure;
    }

    if (fprintf(f, "%lu", (unsigned long)pid) <= 0) {
        fclose(f);
        return failure;
    }

    if (fclose(f))
        return failure;

    return success;
}

// This thread waits for SIGTERM.
static void *shutdown_thread(void *arg)
{
    sigset_t catch;
    int caught;
    process proc = arg;

    assert(proc != NULL);

    pid_t my_pid = gettid();
    if (!write_pid(proc, my_pid))
        warning("Unable to write pid %lu to the pid file.", (unsigned long)my_pid);

    sigemptyset(&catch);
    sigaddset(&catch, SIGTERM);

    // Wait, wait, wait for SIGTERM
    sigwait(&catch, &caught);
    atomic_store_explicit(&proc->shutting_down, true, memory_order_relaxed);

    /* Shut down all services we handle */
    // BUG/TODO: We cannot call shutdown before pthread_join! boa@20251017
    for (size_t i = 0; i < proc->nservices; i++) {
        struct service *s = &proc->services[i];
        s->shutdown_func(s->object);
    }

    return NULL;
}

static status_t start_shutdown_thread(process this)
{
    int err;

    // Block the signals we handle before creating threads so threads
    // inherit the blocks.
    if (!set_signals_to_block()) {
        debug("Could not block signals\n");
        return failure;
    }

    // start the shutdown thread
    if ((err = pthread_create(&this->sdt, NULL, shutdown_thread, this))) {
        debug("Could not create shutdown thread\n");
        return fail(err);
    }

    return success;
}

// If we fail after the shutdown thread has started, we have to kill it.
// That's done here.
static void stop_shutdown_thread(process this)
{
    pthread_kill(this->sdt, SIGTERM);
}

static void *launcher(void *args)
{
    assert(args != NULL);

    struct service *s = args;

    status_t exitcode = s->run_func(s->object);
    return (void*)(intptr_t)exitcode;
}

// Handle the thread creation needed to create a thread
// which starts the actual service.
static status_t start_one_service(struct service *s)
{
    assert(s != NULL);

    return pthread_create(&s->tid, NULL, launcher, s) == 0 ? success : failure;
}

// Calls the undo() function for all services for which the do()
// function has been called. Used if we encounter an error during start up
// and want to undo the startup for services already started.
//
// The pointer to the failed object, failed, may be NULL. This
// value means that the undo() function should be called for all services.
static void process_run_undo_functions(process this, struct service *failed)
{
    size_t i;
    struct service *s;

    for (i = 0; i < this->nservices; i++) {
        s = &this->services[i];
        if (s == failed)
            return;

        if (s->undo_func != NULL)
            s->undo_func(s->object);
    }
}


// Run the services do functions. Undo all functions if one fails.
static status_t process_run_do_functions(process this)
{
    size_t i;
    struct service *s;
    status_t rc = success;

    for (i = 0; i < this->nservices; i++) {
        s = &this->services[i];
        if (s->do_func != NULL && !s->do_func(s->object)) {
            rc = failure;
            break;
        }
    }

    // s points to failed service so we undo any service prior to s.
    if (rc == failure)
        process_run_undo_functions(this, s);

    return rc;
}

static void process_stop_services(process this, struct service *failed)
{
    size_t i;
    struct service *s;

    for (i = 0; i < this->nservices; i++) {
        s = &this->services[i];
        if (s == failed)
            return;

        // Shut down the object and wait to get the exitcode
        s->shutdown_func(s->object);
        assert(s->tid);
        pthread_join(s->tid, NULL);
    }
}

static status_t process_start_services(process this)
{
    size_t i;
    struct service *s;

    assert(this != NULL);

    // Now start the services
    for (i = 0; i < this->nservices; i++) {
        s = &this->services[i];

        if (!start_one_service(s)) {
            // we failed to start one object. Do not start the rest of the
            // services. Call the shutdown function for each object to tell
            // it to stop running.
            process_stop_services(this, s);
            stop_shutdown_thread(this);
            return failure;
        }
    }

    return success;
}

static status_t process_change_rootdir(process this)
{
    assert(this != NULL);

    if (cstring_length(this->rootdir) > 0) {
        const char *rootdir = c_str(this->rootdir);

        if (chdir(rootdir) || chroot(rootdir)) {
            // Unable to change directory.
            stop_shutdown_thread(this);
            process_run_undo_functions(this, NULL);
            debug("Could not change root directory\n");
            return failure;
        }
    }

    return success;
}

status_t process_start(process this, int fork_and_close)
{
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

    if (!process_run_do_functions(this))
        return failure;

    // Start shutdown thread before we do setuid() or chroot() to
    // be able to write to /var/run.
    if (!start_shutdown_thread(this)) {
        process_run_undo_functions(this, NULL);
        return failure;
    }

    // Set current directory and user id if supplied by the caller
    if (cstring_length(this->username) > 0) {
        struct passwd* pw;
        errno = 0; // TODO: Save old value of errno
        if ((pw = getpwnam(c_str(this->username))) == NULL) {
            stop_shutdown_thread(this);
            process_run_undo_functions(this, NULL);

            // Hmm, either the user didn't exist in /etc/passwd or we didn't
            // have permission to read it or we ran out of memory. The Linux
            // man page doesn't mention any other errno value than ENOMEM,
            // which is why we set and test errno before return.
            if (errno == 0)
                errno = ENOENT;

            debug("Could not get username. getpwnam() failed\n");
            return failure;
        }

        if (!process_change_rootdir(this))
            return failure;

        if (setuid(pw->pw_uid)) {
            // Oops, unable to change user id. This is serious since the
            // process may continue running as e.g. root. The safest thing
            // to do is therefore to stop the process.
            process_run_undo_functions(this, NULL);
            stop_shutdown_thread(this);
            debug("Could not set uid\n");
            return failure;
        }
    }
    else if (!process_change_rootdir(this))
        return failure;


    // Now we should be good to go. Start all services.
    return process_start_services(this);
}

// Wait for shutdown thread to finish. That function calls shutdown_func
// for managed services. Our job is to wait for those to finish,
// but the threads themselves must finish too. The semantics are tricky
// as the services run in individual threads so we can call their dtors,
// but we must let them finish before we join the thread. IOW, pthread_join()
// will be our sync point.
//
// Note that shutdown != free. It just means "stop running",
// not "free memory and resources."
status_t process_wait_for_shutdown(process this)
{
    size_t i;
    int err;

    assert(this != NULL);
    assert(this->sdt);

    // Wait for shutdown thread to exit.
    if ((err = pthread_join(this->sdt, NULL)))
        return fail(err);

    // Wait for the started services to finish.
    for (i = 0; i < this->nservices; i++) {
        struct service *s = &this->services[i];
        void *pfoo = &s->exitcode;
        assert(s->tid);

        if ((err = pthread_join(s->tid, &pfoo)))
            return fail(err);
    }

    return success;
}

int process_get_exitcode(process this, void *object)
{
    size_t i;

    assert(this != NULL);
    assert(object != NULL);

    for (i = 0; i < this->nservices; i++) {
        struct service *s = &this->services[i];
        if (s->object == object)
            return (int)s->exitcode;
    }

    return -1;
}


#ifdef CHECK_META_PROCESS

// 20251007: Time to test this fucker. It's a tricky one, so let's start as
// simple as we can.
// 1. Create a dummy object to start and stop. We want to test that
//    semantics are fine and that SIGTERM works as expected!
//
// 2. Test invariants with usernames and chroot, both successful and broken.
//

struct test1 {
    _Atomic bool shutting_down;
    int placeholder;
};

static status_t test1_do(void *vthis)
{
    struct test1 *this = vthis;

    this->placeholder = 0;
    printf("%s()\n", __func__);
    return success;
}

static status_t test1_undo(void *vthis)
{
    struct test1 *this = vthis;

    this->placeholder = 1;
    printf("%s()\n", __func__);
    return success;
}

static status_t test1_run(void *vthis)
{
    struct test1 *this = vthis;

    while (!atomic_load_explicit(&this->shutting_down, memory_order_relaxed)) {
        struct timespec ts = { 0, 100000 };
        nanosleep(&ts, NULL);
    }

    this->placeholder = 2;
    printf("%s()\n", __func__);
    return success;
}

static status_t test1_shutdown(void *vthis)
{
    struct test1 *this = vthis;

    atomic_store_explicit(&this->shutting_down, true, memory_order_relaxed);
    printf("%s()\n", __func__);
    return success;
}

static status_t run_test1(void)
{
    status_t rc;

    process proc = process_new("test1");
    struct test1 *t1 = malloc(sizeof *t1);
    atomic_store_explicit(&t1->shutting_down, false, memory_order_relaxed);

    printf("%s(): tid:%lu pid:%lu\n", __func__, (unsigned long)pthread_self(), (unsigned long)gettid());
    rc = process_add_object_to_start(proc, t1, test1_do, test1_undo,
        test1_run, test1_shutdown);

    if (rc == failure)
        exit(1);

    rc = process_start(proc, 0);
    if (rc == failure)
        exit(2);

    // If we're here, the process is running and has a shutdown thread
    // waiting for SIGTERM. We can call stop_shutdown_thread() which
    // will send SIGTERM to the right tid, but let's sleep for a sec
    // before we do that. This is a lazy way to do things, but it makes
    // it possible to send the signal from within the process and doesn't
    // require the test framework to kill it.
    sleep(1);
    stop_shutdown_thread(proc);

    // Now we need to wait for the shutdown process to finish.
    // This may take a long time IRL. This function will join
    // the process' shutdown thread and then join other services' threads.
    // Let's hope our laziness above doesn't mess things up as we perhaps
    // should've been in a separate thread when calling stop_shutdown_thread()
    rc = process_wait_for_shutdown(proc);
    if (rc == failure)
        exit(3);

    // We can now either inspect exit codes or just free mem
    free(t1);
    process_free(proc);

    return success;
}


int main(void)
{
    run_test1();
    return 0;
}

#endif
