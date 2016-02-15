#include <stdio.h>
#include <unistd.h>

#include <highlander.h>
#include "metal.h"

void foomain(void)
{
    printf("Hello from %s\n", __func__);
}

int main(void)
{
    tid_t tid;

    printf("Hello\n");
    if (!metal_init(0))
        die("Meh. Could not initialize metal.\n");

    if (!metal_task_new(&tid, "foo", 0, foomain))
        die("Could not create task foo. error is %d:%s\n", errno, strerror(errno));

    if (!metal_task_start(tid))
        die("Meh. Could not start a task(%s)\n", strerror(errno));
    sleep(1);

    metal_exit();
    return 0;
}
