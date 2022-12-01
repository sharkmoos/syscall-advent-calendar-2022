#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>


int main(int argc, char *argv[])
{
    // For cat, we have to iterate over all command-line arguments of
    // our process. Thereby, argv[0] is our program binary itself ("./cat").
    int idx;
    for (idx = 1; idx < argc; idx++)
    {
        printf("argv[%d] = %s\n", idx, argv[idx]);
    }
    char buffer[4096];
    int total_size = 0;
    // for each file concatenate it's contents in global variable buffer and write the result to stdout
    for (idx = 1; idx < argc; idx++)
    {
        if (total_size >= 4096)
        {
            printf("Error: buffer overflow. Cannot continue reading  files.\n");
            return 1;
        }
        int fp = open(argv[idx], O_RDONLY);
        if (!fp)
        {
            printf("Error opening file %s\n", argv[idx]);
            return 1;
        }
        int size = read(fp, buffer + total_size, 4096 - total_size);
        total_size += size;
        close(fp);
    }
    printf("%s\n", buffer);
}
