#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <assert.h>
#include <limits.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define die(msg) do { perror(msg); exit(EXIT_FAILURE); } while(0)


/* This function should be the slower implementation, as it does a read and then a write operation, requiring
 * an intermediate buffer and two system calls. I tried to speed it up a little though by using a static buffer,
 * so it won't have to allocate memory on every function call.
 * */

ssize_t copy_write(int fd_in, int fd_out, int *syscalls)
{
    ssize_t ret = 0;
    *syscalls = 0;
    static unsigned int buf_size = 128 * 1024; // As a reference, GNU cp uses an 128KiB buffer to copy data between file descriptors
    static char *buffer = NULL; // static variables keep their value between function calls
    if (!buffer)
    {
        buffer = (char*) malloc(buf_size); // Allocate buffer only once, due to static
        if (!buffer)
            die("malloc");
    }
    int len, written;
    while (1)
    {
        len = read(fd_in, buffer, buf_size); // read data onto the static buffer
        (*syscalls) ++;
         if ( len < 1)
         {
             if (len == 0) // must be at the end if read is zero
                 break;
             else
                 die("read"); // -1 etc is an error
         }

        written = 0;
        while (written < len)
        {
           int w = write(fd_out, buffer + written, len - written);
           (*syscalls) ++;
           if (w < 1)
               die("write");

           written += w;
        }

        ret += written;
    }
    return ret;
}

/* With sendfile, we can copy data between file descriptors without having to read it into a buffer first.
 * This saves us a system call and a memory allocation.
 * */

ssize_t copy_sendfile(int fd_in, int fd_out, int *syscalls) {
    ssize_t ret = 0;
    *syscalls = 0;

    int len;
    while (1)
    {
        len = sendfile(fd_out, fd_in, NULL, INT_MAX); // I guess we can just choose the maximum value for the length?
        (*syscalls) ++;
        if (len < 1)
        {
            if (len == 0) // same as read, if its zero we must be at the end.
                break;
            else
                die("sendfile");
        }
    }
    return ret;
}

// This function measures the given copy implementation.
// fd_in:  file descriptor to copy from
// fd_out: file descriptor to copy to
// banner: Just a nice string to print the output
// copy:   The copy implementation
// returns the number of bytes per second
double measure(int fd_in, int fd_out, char *banner, ssize_t (*copy)(int, int, int*)) {
    // First, we reset our file descriptors.
    // fd_in:  seek to position zero
    // fd_out: truncate the file to zero bytes.
    if (lseek(fd_in, 0, SEEK_SET) < 0) die("lseek");
    if (ftruncate(fd_out, 0) < 0)      die("ftruncate");

    // Measure the start time.
    struct timespec start, end;
    if (clock_gettime(CLOCK_REALTIME, &start) < 0)
        die("clock_gettime");

    // Perform the actual copy. We give the copy function also a
    // pointer to syscalls, where the implementation should count the
    // number of issued system calls.
    int syscalls;
    ssize_t bytes = copy(fd_in, fd_out, &syscalls);


    // Measure the end time
    if (clock_gettime(CLOCK_REALTIME, &end) < 0)
        die("clock_gettime");

    // Calculate the time delta between both points in time.
    double delta = end.tv_sec - start.tv_sec;
    delta += (end.tv_nsec - start.tv_nsec) / 1e9;

    // Print out some nicely formatted message
    printf("[%10s] copied with %.2f MiB/s (in %.2f s, %d syscalls)\n",
           banner, (bytes /delta) / 1024.0 / 1024.0, delta, syscalls);

    return bytes / delta;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s FILE\n", argv[0]);
        return -1;
    }
    // As input file, we open the user-specified file from the command
    // line as a read only file.
    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in < 0) die("open");

    // As an output, we create an anonymous in-memory file. By using
    // such an in-memory file, do not measure the influence of on-disk
    // file systems.
    int fd_out = memfd_create("target", 0);

    // We will run for ten rounds, unless the user specified something
    // else in the ROUNDS environment variable.
    char *ROUNDS = getenv("ROUNDS");
    int rounds = atoi(ROUNDS ? ROUNDS : "10");

    // We run the copy_write algorithm once to warm up the buffer
    // cache. With this, the input file should now, if small enough,
    // reside in in the buffer cache.
    int dummy;
    copy_write(fd_in, fd_out, &dummy);

    // The actual measurement
    double sendfile = 0, write = 0;
    for (int i = 0; i < rounds; i++) {
        sendfile += measure(fd_in, fd_out, "sendfile", copy_sendfile);
        write    += measure(fd_in, fd_out, "read/write", copy_write);
    }

    // Print the average MiB/s for both copy algorithms
    printf("sendfile: %.2f MiB/s, read/write: %.2f MiB/s\n",
           sendfile/rounds/(1024*1024),
           write/rounds/(1024*1024));
}
