#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define PORT      8080
#define MAX_EVENTS 64
#define BUFSIZE    4096

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sfd == -1) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };
    bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(sfd, SOMAXCONN);

    int epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = sfd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);

    struct epoll_event events[MAX_EVENTS];
    printf("Echo server on port %d (epoll)\n", PORT);

    while (1) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == sfd) {
                // New connection
                int cfd = accept4(sfd, NULL, NULL, SOCK_NONBLOCK);
                ev.events = EPOLLIN | EPOLLET;  // edge-triggered
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
                printf("Accepted fd %d\n", cfd);
            } else {
                // Client data
                char buf[BUFSIZE];
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n <= 0) {
                    if (n == 0)
                        printf("Client %d disconnected\n", fd);
                    else
                        perror("read");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    write(fd, buf, n);  // echo
                }
            }
        }
    }

    close(epfd);
    close(sfd);
    return 0;
}
