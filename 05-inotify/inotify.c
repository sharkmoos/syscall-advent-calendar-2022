#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <stdbool.h>
#include <unistd.h>

/* With each inotify event, the kernel supplies us with a bit mask
 * that indicates the cause of the event. With the following table,
 * with one flag per line, you can decode the mask of events.

 If you wonder about the wild syntax of the definition here, a word of
 explanation: Each definition has the form of

    TYPE VARNAME (= INITIALIZER);

 Usually, we use named typed for TYPE (e.g., int, struct bar, long).
 However, C also allows to use unnamed types that are declared in
 place. So in the following

   TYPE        = 'struct { .... } []'
   VARNAME     = inotify_event_flags
   INITIALIZER = { { IN_ACCESS, ...}, ...}
 */
struct {
    int mask;
    char *name;
} inotify_event_flags[] = {
    {IN_ACCESS, "access"},
    {IN_ATTRIB, "attrib"},
    {IN_CLOSE_WRITE, "close_write"},
    {IN_CLOSE_NOWRITE, "close_nowrite"},
    {IN_CREATE, "create"},
    {IN_DELETE, "delete"},
    {IN_DELETE_SELF, "delete_self"},
    {IN_MODIFY, "modify"},
    {IN_MOVE_SELF, "move_self"},
    {IN_MOVED_FROM, "move_from"},
    {IN_MOVED_TO, "moved_to"},
    {IN_OPEN, "open"},
    {IN_MOVE, "move"},
    {IN_CLOSE, "close"},
    {IN_MASK_ADD, "mask_add"},
    {IN_IGNORED, "ignored"},
    {IN_ISDIR, "directory"},
    {IN_UNMOUNT, "unmount"},
};

// We already know this macro from yesterday.
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*(arr)))
#define BUFF_LEN 4096

int main(void) {
    // We allocate a buffer to hold the inotify events, which are
    // variable in size.
    void *buffer = malloc(BUFF_LEN);
    if (!buffer)
    {
        perror("malloc");
        exit(-1);
    }

    // FIXME: Create Inotify Object (into inotify_fd)
    int inotify_fd = inotify_init();
    if (inotify_fd < 0)
    {
        perror("inotify_init");
        return -1;
    }

    // FIXME: Add new watch to that event (result into watch_fd)
    int watch_fd = inotify_add_watch(inotify_fd, ".", IN_OPEN | IN_ACCESS | IN_CLOSE);
    if (watch_fd < 0)
    {
        perror("inotify_add_watch");
        return -1;
    }

    // FIXME: Use read() and the buffer to retrieve results from the inotify_fd
    while ( true )
    {
        // the buffer could contain multiple inotify events
        int read_size = read(inotify_fd, buffer, BUFF_LEN);
        if ( read_size < 0)
        {
            perror("inotify-read");
            return -1;
            break;
        }
        // inotify events are variable in size, do some cleverness to iterate over them
        for (struct inotify_event *event = buffer;
            (char *)event < (char *)buffer + read_size; // linter is going to hate this...
            event = (struct inotify_event*)event + sizeof(struct inotify_event) + event->len
                    )
        {
            // not all inotify events will have a name (file name). Those are directory events
            if ( event -> len != 0 )
            {
                printf("Inotify event: [%s]    ", event->name);
            }
            else
            {
                printf("Inotify event: [.]    ");
            }

            char *deliminator = " | ";
            for (int i = 0; i < ARRAY_SIZE(inotify_event_flags); i++)
            {
                if (event->mask & inotify_event_flags[i].mask)
                {
                    printf("%s%s", deliminator, inotify_event_flags[i].name);
                }
            }
            printf("\n");

        }
    }

    // As we are nice, we free the buffer again.
    free(buffer);
    return 0;
}
