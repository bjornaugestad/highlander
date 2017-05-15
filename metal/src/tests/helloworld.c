#include <stdio.h>
#include <unistd.h>

#include <metal.h>

#define MM_MY_MESSAGE (MM_USER_BASE + 1)

void hellofn(void)
{
    tid_t sender;
    msgid_t msg;
    msgarg_t arg1, arg2;

    static int i;

    while (message_get(&sender, &msg, &arg1, &arg2) == success) {
        switch (msg) {
            case  MM_MY_MESSAGE:
                printf("Hello, world!(%d)\n", i++);
                break;

            case MM_EXIT:
                puts("Exiting");
                goto end;
        }
    }

end: // Not much to do here.
    return;
}


int main(void)
{
    tid_t tid;
    size_t i, n;

    if (!metal_init(0))
        return 1;

    if (!metal_task_new(&tid, "hello", 0, hellofn))
        die("Could not create task.\n");

    if (!metal_task_start(tid))
        die("Could not start task\n");

    usleep(1500);

    n = 100;
    for (i = 0; i < n; i++) {
        if (!message_send(0, tid, MM_MY_MESSAGE, 0, 0))
            die("Could not send message to task\n");
    usleep(1000);
    }

    usleep(1000);

    // Stop the task in a civilized way
    if (!metal_task_stop(tid))
        die("Could not stop task");

    if (!metal_exit())
        return 1;

    return 0;
}
