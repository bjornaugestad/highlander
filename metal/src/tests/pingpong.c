/* Pingpong sends a message back and forth between two tasks.
 * Pingpong requires a working subscription scheme, which we
 * don't have yet. Pingpong also requires support for 
 * multiple instances of tasks, which we don't have either.
 * IOW, we need to do some work before pingpong is up and running.
 */
#include <stdio.h>
#include <unistd.h>

#include <metal.h>

#define MM_MY_MESSAGE (MM_USER_BASE + 1)

void pingpongfn(void)
{
    tid_t sender;
    msgid_t msg;
    msgarg_t arg1, arg2;

    static int i;
    unsigned long n = 0;

    while (message_get(&sender, &msg, &arg1, &arg2) == success) {
        switch (msg) {
            case MM_MY_MESSAGE:
                n++;
                if (!message_publish(msg, arg1 + 1, arg2)) {
                    puts("Could not publish message");
                    break; // Do nothing ATM
                }
                break;

            case MM_EXIT:
                goto end;
        }
    }

end: // Not much to do here.
    printf("Got %ld messages. Good bye\n", n);
    return; 
}

int main(void)
{
    tid_t tid1, tid2;
    size_t i, n;

    if (!metal_init(0))
        return 1;

    if (!metal_task_new(&tid1, "pingpong", 0, pingpongfn)
    ||  !metal_task_new(&tid2, "pingpong", 1, pingpongfn))
        die("Could not create tasks.\n");

    // Set them up to subscribe to each other
    if (!metal_subscribe(tid1, tid2)
    || !metal_subscribe(tid2, tid1))
        die("Could not subscribe.");

    if (!metal_task_start(tid1) || !metal_task_start(tid2))
        die("Could not start tasks\n");

    if (!message_send(0, tid1, MM_MY_MESSAGE, 1, 0))
        die("Could not send message to task\n");

    // Wait a long time.
    usleep(10 * 1000 * 1000);

    // Stop the tasks in a civilized way
    if (!metal_task_stop(tid1) || !metal_task_stop(tid2))
        die("Could not stop tasks");

    usleep(100);

    if (!metal_exit())
        return 1;

    return 0;
}
