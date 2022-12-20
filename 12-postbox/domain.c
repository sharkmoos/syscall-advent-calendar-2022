/* Created the socket at /tmp/socket rather than ./socket because the
 * Windows file system does not support sockets so using WSL
 * */

int domain_prepare(int epoll_fd)
{
    printf("... by socket: echo 2 | nc -U socket\n");
    // open the socket file descriptor
    int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        printf("Socket open failed\n");
        return -1;
    }

    // create the socket struct
    char *socket_name = "socket";
    struct sockaddr_un server_socket_addr;
    strcpy(server_socket_addr.sun_path, socket_name);
    server_socket_addr.sun_family = AF_UNIX;

    // unlink the socket
    if ( unlink(socket_name) < 0 && errno != ENOENT )
    {
        printf("unlink socket failed\n");
        return -1;
    }

    // bind the socket to the file descriptor
    if ( bind(socket_fd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) == -1 )
    {
        printf("bind socket failed\n");
        return -1;
    }

    // listen for connections
    if ( listen(socket_fd, 10) < 0 )
    {
        printf("listen socket failed\n");
        return -1;
    }

    // add the fifo fd to the epoll list
    epoll_add(epoll_fd, socket_fd, EPOLLIN);
    return socket_fd;
}


void domain_accept(int epoll_fd, int sock_fd, int events)
{
    // accept the connection and add to the epoll
    int client_fd = accept(sock_fd, NULL, NULL);
    if (client_fd < 0)
    {
        printf("accept failed\n");
        return;
    }
    epoll_add(epoll_fd, client_fd, EPOLLIN);
}


void domain_recv(int epoll_fd, int sock_fd, int events)
{
    static char buf[1024];
    if ( EPOLLIN & events )
    {
        // read the message from the socket
        int len = recv(sock_fd, buf, sizeof(buf), 0);
        if (  len < 0 )
        {
           printf("recv failed\n");
           return;
        }

        while ( len > 0 && buf[len-1] == '\n' )
            len--;
        buf[len-1] = '\0';

        // retrieve the socket options (getsockopt) and print the message
        struct ucred ucred; // struct to hold the socket options
        socklen_t ucred_len = sizeof(ucred);
        if ( getsockopt(sock_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &ucred_len) < 0 )
        {
            printf("getsockopt failed\n");
            return;
        }

        printf("Received request from pid:%d uid:%d gid:%d: %s\n", ucred.pid, ucred.uid, ucred.gid, buf);
        epoll_del(epoll_fd, sock_fd);
        close(sock_fd);

    }
    else if ( EPOLLHUP & events )
    {
        epoll_del(epoll_fd, sock_fd);
        close(sock_fd);
    }
    else
    {
        printf("Unknown event\n");
    }
}
