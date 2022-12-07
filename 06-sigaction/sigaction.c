#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ucontext.h>

int PAGE_SIZE;

extern int main(void);

/* See Day 2: clone.
 *
 * Example: syscall_write("foobar = ", 23);
 */
int syscall_write(char *msg, int64_t number, char base) {
    write(1, msg, strlen(msg));

    char buffer[sizeof(number) * 8];
    char *p = &buffer[sizeof(number) * 8];
    int len = 1;
    *(--p) = '\n';
    if (number < 0) {
        write(1, "-", 1);
        number *= -1;
    }
    do {
        *(--p) =  "0123456789abcdef"[number % base];
        number /= base;
        len ++;
    } while (number != 0);
    write(1, p, len);
    return 0;
}


/* We have three different fault handlers to make our program
 * nearly "immortal":
 *
 * 1. sa_sigint:  Is invoked on Control-C.
 * 2. sa_sigsegv: Handle segmentation faults
 * 3. sa_sigill:  Jump over illegal instructions
*/

volatile bool do_exit = false;

/*
According to the internet (https://stackoverflow.com/questions/72157223/how-do-i-implement-multiple-signal-handlers-using-sigaction)
different signal handlers can be implemented using sigaction by simple channging the sa_sigaction argument so that
it points to the correct handler.

 This says you need to call sigaction multiple times
 https://www.linuxquestions.org/questions/programming-9/catching-multiple-signals-in-a-single-handler-in-linux-c-877096/
 */

// 1. Handle Segmentation faults by setting up a SIGSEGV handler that maps pages to info->si_addr.
void sa_sigsegv(int signum, siginfo_t *signal_info, void *ptr)
{
    // print the memory address that accessed to cause the signal handler to trigger
    syscall_write("[sigaction(SIGSEGV)] si_addr = 0x", (intptr_t) signal_info->si_addr, 16);

    // calculate the memory address from the fault
    uintptr_t fault_addr = (intptr_t) signal_info->si_addr;
    uintptr_t page_addr = fault_addr & ~(PAGE_SIZE - 1);

    // map a new (anonymous) page at the fault address
    if ( mmap((void *) page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == MAP_FAILED )
    {
        syscall_write("[sigaction(SIGSEGV)] mmap failed for address 0x", page_addr, 16);
        exit(-1);
    }
    syscall_write("[sigaction(SIGSEGV)] mmap(PAGESIZE) succeeded: 0x", page_addr, 16);
}

// 2. Handle illegal instructions by setting up a SIGILL handler that jumps 4 bytes forward and hopefully
// over the illegal instruction.
void sa_sigill(int signum, siginfo_t *signal_info, ucontext_t* context)
{
    // I think this is architecture specific
    // Would be interesting to see if this works on ARM
    uintptr_t program_counter = context->uc_mcontext.gregs[REG_RIP];
    syscall_write("[sigaction(SIGILL)] Illegal Instructions @ RIP = 0x", program_counter, 16);

    // set the program counter to the next instruction
    // super unrealistic to expect the instruction to be 4 bytes long in x86, but who cares I guess
    // I wont complain about not having to write a disassembler
    // I guess it would be **way** easier in ARM32
    program_counter += 4;
    syscall_write("[sigaction(SIGILL)] Setting RIP = 0x", program_counter, 16);
    context->uc_mcontext.gregs[REG_RIP] = program_counter;
}

// 3. Handle Control-C by setting up a SIGINT handler that sets a global variable to true.
void sa_sigint(int signum, siginfo_t *signal_info, void *ptr)
{
    syscall_write("[sigaction(SIGINT)] Caught SIGINT", 0, 10);
    do_exit = true;
}

int main(void) {
    // We get the actual page-size for this system. On x86, this
    // always return 4096, as this is the size of regular pages on
    // this architecture. We need this in the SIGSEGV handler.
    PAGE_SIZE = sysconf(_SC_PAGESIZE);


    // signal handler for SIGSEGV
    struct sigaction signal_action;
    memset(&signal_action, 0, sizeof(signal_action));
    signal_action.sa_flags = SA_SIGINFO | SA_RESTART; // man, (not having) this trolled me for a while
    sigemptyset(&signal_action.sa_mask);

    int signal_success[3] = {};

    signal_action.sa_sigaction = &sa_sigsegv;
    signal_success[0] = sigaction(SIGSEGV, &signal_action, NULL);

    signal_action.sa_sigaction = &sa_sigill;
    signal_success[1] = sigaction(SIGILL, &signal_action, NULL);

    signal_action.sa_sigaction = &sa_sigint;
    signal_success[2] = sigaction(SIGINT, &signal_action, NULL);

    for ( int i = 0; i < sizeof(signal_success); i++ )
    {
        char *signal_names[] = {"SIGINT", "SIGILL", "SIGSEGV"};
        if (signal_success[i] == -1)
        {
            printf("Error: sigaction() failed for signal %s", signal_names[i]);
            return -1;
        }
    }

    // since the program will segfault after this, nice to check the signal installation didn't cause the crash
    printf("Signal handlers installed successfully\n");


    // We generate an invalid pointer that points _somewhere_! This is
    // undefined behavior, and we only hope for the best here. Perhaps
    // we should install a signal handler for SIGSEGV beforehand....
    uint32_t * addr = (uint32_t*)0xdeadbeef;

    // This will provoke a SIGSEGV
    *addr = 23;

//     Two ud2 instructions are exactly 4 bytes long
    #define INVALID_OPCODE_32_BIT() asm("ud2; ud2;")

//     This will provoke a SIGILL
    INVALID_OPCODE_32_BIT();

//     Happy faulting, until someone sets the do_exit variable.
//     Perhaps the SIGINT handler?
    while(!do_exit) {
        sleep(1);
        addr += 22559;
        *addr = 42;
        INVALID_OPCODE_32_BIT();
    }

    { // Like in the mmap exercise, we use pmap to show our own memory
      // map, before exiting.
        char cmd[256];
        snprintf(cmd, 256, "pmap %d", getpid());
        printf("---- system(\"%s\"):\n", cmd);
        system(cmd);
    }

    return 0;
}
