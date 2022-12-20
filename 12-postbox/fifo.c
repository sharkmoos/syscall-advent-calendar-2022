/* Created the fifo at /tmp/fifo rather than ./fifo because the
 * Windows file system does not support named pipes so using WSL.
 * */


int fifo_prepare(int epoll_fd)
{
    printf("... by fifo:   echo 1 > fifo\n");

    // unlink the file
    if ( unlink("/tmp/fifo") < 0 && errno != ENOENT )
    {
        printf("unlink fifo failed\n");
        return -1;
    }

    // create the fifo using mknod
    if ( mknod("/tmp/fifo", S_IFIFO | 0666, 0) < 0 )
    {
        printf("mknod fifo failed\n");
        return -1;
    }

    // create a file descriptor for the fifo
    int fifo_fd = open("/tmp/fifo", O_RDONLY | O_NONBLOCK);
    if ( fifo_fd < 0 )
    {
        printf("open fifo failed\n");
        return -1;
    }

    epoll_add(epoll_fd, fifo_fd, EPOLLIN);
    return fifo_fd;
}

void fifo_handle(int epoll_fd, int fifo_fd, int events)
{
    static char buf[1024];
    if (events & EPOLLIN)
    {
        // read from the fifo
        ssize_t len = read(fifo_fd, buf, sizeof(buf));
        if (len < 0)
        {
            printf("read fifo failed");
            return;
        }
        else if (len > 1)
        {
            while (len > 1 && buf[len - 1] == '\n')
                len--;
            buf[len] = '\0';
            printf("fifo: %s", buf);
        }
        else if ( len == 0 )
        {
            // the fifo has been closed
            goto close;
        }
    }
    else if (events & EPOLLHUP)
    {
        close:
            epoll_del(epoll_fd, fifo_fd);
            close(fifo_fd);

            fifo_fd = open("/tmp/fifo", O_RDONLY | O_NONBLOCK);
            epoll_add(epoll_fd, fifo_fd, EPOLLIN);
    }
}
