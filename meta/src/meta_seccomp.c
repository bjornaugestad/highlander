#include <assert.h>
#include <stddef.h>
#include <seccomp.h>

#include <meta_seccomp.h>

// create a permission list, call init() and rule_add(), load(),  and return ctx. 
scmp_filter_ctx meta_drop_perms(const int *perms_to_keep)
{
    // Permissions for the shutdown thread. As we can see, 
    // sanitizers are not ideal as they're greedy perm bastards.
    static const int analyzer_perms[] = {
#ifndef __SANITIZER_UNDEFINED__
        SCMP_SYS(mmap),           // Memory allocation, needed by sanitizers to allocate memory regions (e.g., ASan, MSan)
        SCMP_SYS(munmap),         // Memory deallocation, needed by sanitizers to free memory regions (e.g., ASan, MSan)
        SCMP_SYS(mprotect),       // Memory protection, used by ASan to mark memory as readable/writable/executable
        SCMP_SYS(futex),          // Thread synchronization, needed by TSan for detecting race conditions and synchronization errors
        SCMP_SYS(gettid),         // Thread ID, needed by TSan for distinguishing between threads
        SCMP_SYS(clock_gettime),  // Time fetching, used by TSan for tracking time-related events in threads
        SCMP_SYS(clock_nanosleep),// Time sleeping, necessary for TSan to track thread sleep/wait events
        SCMP_SYS(exit),           // Exit system call, needed to terminate a thread or process (e.g., after a sanitizer error)
        SCMP_SYS(exit_group),     // Exit group, used to terminate all threads in the process (e.g., after a sanitizer error)
        SCMP_SYS(getpid),         // Process ID, used to identify the process (important for error reporting and tracking)
        SCMP_SYS(rt_sigaction),   // Signal handling, necessary for UBSan and ASan to handle errors via signals
        SCMP_SYS(rt_sigprocmask), // Signal masking, used by sanitizers to block or unblock signals (important for error reporting)
        SCMP_SYS(write),          // Used for logging error messages (important for ASan, UBSan, etc.)
        SCMP_SYS(open),           // File access, may be needed for logging or error reporting
        SCMP_SYS(close),          // Closing file descriptors, used by sanitizers when cleaning up resources
        SCMP_SYS(read),           // Reading input, can be used by sanitizers for logging or input-related checks
        SCMP_SYS(brk),            // Adjust heap size, used in systems that rely on brk/sbrk for memory management
        SCMP_SYS(sigaltstack),
#endif
        -1,
    };

    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_TRAP);

    // Add analyzer perms if needed
    const int *p = analyzer_perms;
    while (*p != -1) 
        seccomp_rule_add(ctx, SCMP_ACT_ALLOW, *p++, 0);

    // Now add more perms
    p = perms_to_keep;
    while (*p != -1) 
        seccomp_rule_add(ctx, SCMP_ACT_ALLOW, *p++, 0);

    seccomp_load(ctx);
    return ctx;
}

void  meta_release_perms(void *vctx)
{
    assert(vctx != NULL);
    seccomp_release(vctx);
}
