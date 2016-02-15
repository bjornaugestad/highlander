#ifndef METAL_H
#define METAL_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include <meta_common.h>

#include <metal_messages.h>

// some types
// The Metal Task id type. Use a big integer so we
// can create lots and lots of tasks without wrapping.
typedef unsigned long tid_t;

// Message ids are unique
typedef unsigned msgid_t;

// We want to be able to send pointers as arguments to a message.
typedef uintptr_t msgarg_t;

// Function prototype for the task's main function.
typedef void (*taskfn)(void);

// Initialize the metal library
status_t metal_init(int flags) 
    __attribute__((warn_unused_result));

status_t metal_exit(void);

// Create a new task and return the task's id in the tid arg.
status_t metal_task_new(tid_t *tid, const char *name, taskfn fn) 
    __attribute__((warn_unused_result));

status_t metal_task_start(tid_t tid) 
    __attribute__((warn_unused_result));

status_t metal_task_stop(tid_t tid) 
    __attribute__((warn_unused_result));

// Messages : We want to send and receive messages.
// Strictly speaking we publish events and subscribe to events.
// For all practical purposes, a message is an event.
status_t message_get(tid_t *sender, msgid_t *msg, msgarg_t *arg1, msgarg_t *arg2) 
    __attribute__((warn_unused_result));

status_t message_send(tid_t sender, tid_t dest, msgid_t msg, msgarg_t arg1, msgarg_t arg2) 
    __attribute__((warn_unused_result));

status_t message_publish(msgid_t msg, msgarg_t arg1, msgarg_t arg2) 
    __attribute__((warn_unused_result));

// Functions to subscribe to tasks' events
status_t metal_subscribe(tid_t publisher, tid_t subscriber) 
    __attribute__((warn_unused_result));

status_t metal_unsubscribe(tid_t publisher, tid_t subscriber) 
    __attribute__((warn_unused_result));


// Task functions
typedef struct task_tag *task;
task task_new(void);
status_t task_init(task p, const char *name, taskfn fn, tid_t tid);
task self(void);

void task_free(task p);
status_t task_start(task p) __attribute__((nonnull(1)));

status_t task_message_add(task p, tid_t sender, msgid_t msg, msgarg_t arg1, msgarg_t arg2);

void task_set_name(task p, const char *name) __attribute__((nonnull(1)));
void task_set_tid(task p, tid_t tid) __attribute__((nonnull(1)));
void task_set_fn(task p, taskfn fn) __attribute__((nonnull(1)));

const char* task_name(task p) __attribute__((nonnull(1)));
tid_t task_tid(task p) __attribute__((nonnull(1)));
tid_t self_tid(void);
pthread_t task_threadid(task p);

#endif
