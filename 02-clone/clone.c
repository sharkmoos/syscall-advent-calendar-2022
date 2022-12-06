#define _GNU_SOURCE
#include <sched.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

/* For our clone experiments, we working on a very low level and
 * fiddle around with threading. However, this leads to a problem with
 * the libc, which must perform some user-space operations to setup a
 * new thread. For example, in a clone()ed thread, we cannot simply
 * use printf(). Therefore, we provide you with a simple function to
 * output a string and a number. The writing to stdout (fd=1) is done
 * with plain system calls.
 *
 * Example: syscall_write("foobar = ", 23);
 */
int syscall_write(char *msg, int number) {
    write(1, msg, strlen(msg));
    if (number != 0) {
        char buffer[sizeof(number) * 3];
        char *p = &buffer[sizeof(number) * 3];
        int len = 1;
        *(--p) = '\n';
        if (number < 0) {
            write(1, "-", 1);
            number *= -1;
        }
        while (number > 0) {
            *(--p) =  (number % 10) + '0';
            number /= 10;
            len ++;
        }
        write(1, p, len);
    } else {
        write(1, "0\n", 2);
    }


    return 0;
}

// For the new task, we always require an stack area. To make our life
// easier, we just statically allocate an global variable of PAGE_SIZE.
char stack[4096];

// To demonstrate whether child and parent are within the same
// namespace, we count up a global variable. If they are within the
// same address space, we will see modification to this counter in
// both namespaces
volatile int counter = 0;

int child_entry(void* arg) {
    if (arg != NULL)
    {
        // overwrite the uid namespace map with the value in arg
        int fd = open("/proc/self/uid_map", O_WRONLY);
        if (fd < 0)
        {
            printf("Error opening uid_map: %d", fd);
            return 1;
        }
        write(fd, arg, strlen(arg));
        close(fd);
        setuid(0);
    }
    // We just give a little bit of information to the user.
    syscall_write(": Hello from child_entry", 0);
    syscall_write(": getppid() = ", getppid()); // What is our parent PID
    syscall_write(": getpid()  = ", getpid());  // What is our thread group/process id
    syscall_write(": gettid()  = ", gettid());  // The ID of this thread!
    syscall_write(": getuid()  = ", getuid());  // What is the user id of this thread.


    // We increment the global counter in one second intervals. If we
    // are in our own address space, this will have no influence on
    // the parent!
    while (counter < 4) {
        counter++;
        sleep(1);
    }

    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s MODE\n", argv[0]);
        printf("MODE:\n");
        printf("  - fork    -- emulate fork with clone\n");
        printf("  - chimera -- create process/thread chimera\n");
        printf("  - thread  -- create a new thread in a process\n");
        printf("  - user    -- create a new process and alter its UID namespace\n");
        return -1;
    }

    syscall_write("> Hello from main!", 0);
    syscall_write("> getppid() = ", getppid());
    syscall_write("> getpid()  = ", getpid());
    syscall_write("> gettid()  = ", gettid());
    syscall_write("> getuid()  = ", getuid());

    int flags = 0;
    void *arg = NULL;
    if (strcmp(argv[1], "fork") == 0)
    {
        flags = SIGCHLD;
    }
    else if (strcmp(argv[1], "chimera") == 0)
    {
        // If CLONE_VM is set, the calling process and the  child  process  run  in  the  same memory  space.
        flags = CLONE_VM | SIGCHLD;
    }
    else if (strcmp(argv[1], "thread") == 0)
    {
        // If CLONE_SIGHAND is set, the calling process and the child process share the same signal handlers.
        // If CLONE_THREAD is set, the child is placed in the same thread group as the calling process.
        flags = CLONE_THREAD | CLONE_VM | CLONE_SIGHAND; //
    }
    else if (strcmp(argv[1], "user") == 0)
    {
        flags = CLONE_NEWUSER | SIGCHLD;
        char pid_map[20];
        sprintf(pid_map,"0 %d 1\n", getuid());
        arg = pid_map;
    }
    else
    {
    pid_t pid = clone(child_entry, &stack[4096-1], flags, arg);
    if (pid == -1)
    {
        printf("clone() failed: %s\n");
        return -1;
    }

    syscall_write("\n!!!!! Press C-c to terminate. !!!!!", 0);
    while(counter < 4)
    {
        syscall_write("counter = ", counter);
        sleep(1);
    }
    return 0;
}
