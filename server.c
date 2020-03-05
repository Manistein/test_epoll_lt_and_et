#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

static int do_listen(const char* host, int port) {
    struct addrinfo ai_hints;
    struct addrinfo* ai_list = NULL;
    char portstr[16];
    sprintf(portstr, "%d", port);
    memset(&ai_list, 0, sizeof(ai_hints));

    ai_hints.ai_socktype = SOCK_STREAM;
    ai_hints.ai_protocol = IPPROTO_TCP;

    int status = getaddrinfo(host, portstr, &ai_hints, &ai_list);
    if (status != 0) {
        return -1;
    }

    int fd = socket(ai_list->ai_family, ai_list->ai_socktype, 0);
    if (fd < 0) {
        freeaddrinfo(ai_list);
        return -1;
    }

    status = bind(fd, (struct sockaddr*)ai_list->ai_addr, ai_list->ai_addrlen);
    if (status != 0) {
        close(fd);
        freeaddrinfo(ai_list);
        return -1;
    }

    listen(fd, 30);

    printf("do_listen success fd:%d\n", fd);
    return fd;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("please input mode, lt or et\n");
        return -1;
    }

    int ep_event = 0;
    if (strcmp(argv[1], "et") == 0) {
        ep_event = EPOLLET;
    }
    else if (strcmp(argv[1], "lt") == 0) {
        ep_event = 0;
    }
    else {
        printf("unknow mode %s please input lt or et\n", argv[1]);
        return -1;
    }


    int epfd = epoll_create(1024);
    if (epfd == -1) {
        printf("fail to create epoll %d\n", errno);
        return -1;
    }

    int listen_fd = do_listen("127.0.0.1", 8001);
    if (listen_fd < 0) {
        printf("do listen fail");
        return -1;
    }

    struct epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ee);

    for(;;) {
        printf("before epoll epoll_wait\n");
        struct epoll_event ev[16];
        int n = epoll_wait(epfd, ev, 16, -1);
        if (n == -1) {
            printf("epoll_wait error %d", errno);
            break;
        }

        printf("after epoll_wait event n:%d\n", n);

        for (int i = 0; i < n; i ++) {
            struct epoll_event* e = &ev[i];
            if (e->data.fd == listen_fd) {
                struct sockaddr s;
                socklen_t len = sizeof(s);
                int client_fd = accept(listen_fd, &s, &len);
                if (client_fd < 0) {
                    break;
                }

                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                ee.events = EPOLLIN | ep_event;
                ee.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ee);
                printf("accpet new connection fd:%d\n", client_fd);
            }
            else {
                int client_fd = e->data.fd;
                int flag = e->events;
                int r = (flag & EPOLLIN) != 0;
                if (r) {
                    printf("read fd:%d\n", client_fd);
                    char buffer[2] = {0};
                    int n = read(client_fd, buffer, 2);
                    printf("read number %d\n", n);
                    if (n < 0) {
                        switch(errno) {
                            case EINTR: break;
                            case EWOULDBLOCK: break;
                            default: break;
                        }
                    }
                    else if (n == 0) {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                        break;
                    }
                    else {
                        printf("----%c%c\n", buffer[0], buffer[1]);
                    }
                }
            }
        }
    }

    return 0;
}
