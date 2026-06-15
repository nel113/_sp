# Chapter 7: Network Programming

## The Berkeley Sockets API

Sockets are the Unix abstraction for network communication. A socket represents one endpoint of a communication channel. The API originated in 4.2BSD (1983) and remains largely unchanged.

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

### Domains

| Domain | Protocol | Address Family |
|--------|----------|----------------|
| `AF_INET` | IPv4 | 32-bit addresses + ports |
| `AF_INET6` | IPv6 | 128-bit addresses + ports |
| `AF_UNIX` | Local | Filesystem path |
| `AF_NETLINK` | Netlink | Kernel-userspace IPC |
| `AF_PACKET` | Raw | Link-layer (requires `CAP_NET_RAW`) |

### Types

| Type | Behavior |
|------|----------|
| `SOCK_STREAM` | Reliable, ordered, connection-oriented (TCP) |
| `SOCK_DGRAM` | Unreliable, unordered, connectionless (UDP) |
| `SOCK_RAW` | Raw IP packets (requires `CAP_NET_RAW`) |
| `SOCK_SEQPACKET` | Reliable, ordered, record-oriented |

## A TCP Echo Server

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(void) {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) { perror("socket"); exit(1); }

    // Allow immediate reuse of the port after restart
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(8080),
        .sin_addr.s_addr = INADDR_ANY,  // 0.0.0.0
    };

    bind(sfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sfd, 5);  // backlog

    while (1) {
        int cfd = accept(sfd, NULL, NULL);  // blocks
        if (cfd == -1) { perror("accept"); continue; }

        char buf[4096];
        ssize_t n = recv(cfd, buf, sizeof(buf), 0);
        if (n > 0) send(cfd, buf, n, 0);    // echo

        close(cfd);
    }
}
```

This is a single-threaded, one-connection-at-a-time server. Real servers need concurrency.

## TCP Client

```c
struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port   = htons(8080),
};
inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

int cfd = socket(AF_INET, SOCK_STREAM, 0);
connect(cfd, (struct sockaddr*)&addr, sizeof(addr));

const char *msg = "Hello, server!";
send(cfd, msg, strlen(msg), 0);

char buf[1024];
ssize_t n = recv(cfd, buf, sizeof(buf), 0);
buf[n] = '\0';
printf("Server replied: %s\n", buf);

close(cfd);
```

## Byte Ordering

Network byte order is **big-endian**. x86-64 is **little-endian**. Always convert:

```c
#include <arpa/inet.h>

uint16_t htons(uint16_t host16);   // host → network short (16-bit)
uint32_t htonl(uint32_t host32);   // host → network long (32-bit)
uint16_t ntohs(uint16_t net16);    // network → host short
uint32_t ntohl(uint32_t net32);    // network → host long

// Address conversion
int inet_pton(int af, const char *src, void *dst);  // string → binary
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
```

```c
addr.sin_port = htons(8080);               // port: host → network
inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);  // IP → binary

char ipstr[INET_ADDRSTRLEN];
inet_ntop(AF_INET, &addr.sin_addr, ipstr, sizeof(ipstr));
printf("Connected to %s\n", ipstr);
```

## Non-blocking I/O

Call `recv()` on a socket with no data, and it blocks your whole thread. Set `O_NONBLOCK` to return immediately with `EAGAIN` or `EWOULDBLOCK`:

```c
int flags = fcntl(sockfd, F_GETFL, 0);
fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
```

Or at socket creation:
```c
int sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
```

But non-blocking alone is not enough — you need a mechanism to wait for readiness on multiple sockets at once.

## I/O Multiplexing

### select()

The oldest (and most limited) multiplexing API:

```c
#include <sys/select.h>

fd_set readfds;
FD_ZERO(&readfds);
FD_SET(sockfd, &readfds);
FD_SET(stdin_fd, &readfds);

int maxfd = sockfd > stdin_fd ? sockfd : stdin_fd;
select(maxfd + 1, &readfds, NULL, NULL, NULL);  // block

if (FD_ISSET(sockfd, &readfds)) { /* data on sockfd */ }
if (FD_ISSET(stdin_fd, &readfds)) { /* stdin ready */ }
```

Limitations: max 1024 fds (on Linux, `FD_SETSIZE`), O(n) scanning, overwrites fd sets on each call.

### poll()

Better than select, but still O(n):

```c
#include <poll.h>

struct pollfd fds[] = {
    { .fd = sockfd,  .events = POLLIN },
    { .fd = stdin_fd, .events = POLLIN },
};

poll(fds, 2, -1);  // block indefinitely

if (fds[0].revents & POLLIN) { /* sockfd readable */ }
if (fds[1].revents & POLLIN) { /* stdin readable */ }
```

`pollfd` struct:
- `events` — requested events (input): `POLLIN`, `POLLOUT`, `POLLERR`, `POLLHUP`.
- `revents` — returned events (output): same flags, plus `POLLNVAL` (invalid fd).

### epoll (Linux-specific)

`epoll` is O(1) for event delivery — it remembers the interest set in the kernel:

```c
#include <sys/epoll.h>

int epfd = epoll_create1(0);

struct epoll_event ev = {
    .events   = EPOLLIN,     // read-ready
    .data.fd  = sockfd,       // application data
};
epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

struct epoll_event events[64];
while (1) {
    int nfds = epoll_wait(epfd, events, 64, -1);  // block
    for (int i = 0; i < nfds; i++) {
        if (events[i].data.fd == sockfd) {
            // sockfd is ready — accept or recv
        }
    }
}

close(epfd);
```

Modes:
- **Level-triggered** (default): reports readiness as long as the condition holds.
- **Edge-triggered** (`EPOLLET`): reports readiness once per state change. You must drain the fd completely (read/write until `EAGAIN`). Higher performance, harder to get right.

```c
ev.events = EPOLLIN | EPOLLET;  // edge-triggered
```

### kqueue (BSD/macOS)

The BSD equivalent of epoll:

```c
#include <sys/event.h>
int kq = kqueue();
struct kevent ev;
EV_SET(&ev, sockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
kevent(kq, &ev, 1, NULL, 0, NULL);
```

Not portable between Linux and BSD — use an abstraction library (libuv, libevent) for cross-platform code.

## Building a Concurrent TCP Server

Three common architectures:

### 1. Fork-per-Connection
```c
while (1) {
    int cfd = accept(sfd, NULL, NULL);
    if (fork() == 0) {
        close(sfd);    // child doesn't need listening socket
        handle_client(cfd);
        close(cfd);
        _exit(0);
    }
    close(cfd);        // parent doesn't need this client
    waitpid(-1, NULL, WNOHANG);  // reap zombies
}
```

Simple but heavy — each connection costs a process.

### 2. Thread-per-Connection
```c
while (1) {
    int *cfd_ptr = malloc(sizeof(int));
    *cfd_ptr = accept(sfd, NULL, NULL);
    pthread_t tid;
    pthread_create(&tid, NULL, client_handler, cfd_ptr);
    pthread_detach(tid);  // auto-cleanup on exit
}
```

Lighter than fork but still O(N) threads for N connections.

### 3. Event-Driven (epoll)
```c
// Single thread, non-blocking, epoll-based
// Handles thousands of connections with minimal overhead
// Used by nginx, Redis, Node.js (libuv)
```

The event-driven model is the modern standard for high-concurrency servers. Combine with thread pools for CPU-bound work.

## Socket Options

```c
int optval;

// Reuse address (avoid TIME_WAIT bind failures)
int reuse = 1;
setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

// Keep-alive probes
int keepalive = 1;
setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

// TCP_NODELAY — disable Nagle's algorithm (send immediately)
int nodelay = 1;
setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

// Receive/send buffer sizes
int bufsize = 256 * 1024;  // 256KB
setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));

// Cork — batch small writes (Linux only)
int cork = 1;
setsockopt(sfd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork));
```

## getaddrinfo: DNS Resolution Done Right

Never use `gethostbyname()` — it's not thread-safe and doesn't support IPv6. Use `getaddrinfo()`:

```c
#include <netdb.h>

struct addrinfo hints = {
    .ai_family   = AF_UNSPEC,       // IPv4 or IPv6
    .ai_socktype = SOCK_STREAM,     // TCP
};
struct addrinfo *result;

int err = getaddrinfo("example.com", "80", &hints, &result);
if (err != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    exit(1);
}

// Try each address returned
for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
    int sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1) continue;

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
        freeaddrinfo(result);
        return sfd;  // connected
    }
    close(sfd);
}

freeaddrinfo(result);
```

## sendfile: Zero-Copy File Transfer

Send a file to a socket without copying data through user space:

```c
#include <sys/sendfile.h>

off_t offset = 0;
ssize_t sent = sendfile(sockfd, file_fd, &offset, file_size);
```

The kernel uses the DMA engine to splice pages directly from the page cache into the socket buffer. This is how nginx serves static files efficiently.

## Raw Sockets and Packet Capture

Raw sockets give you access to the IP layer (or link layer) for packet crafting and sniffing:

```c
// IPPROTO_RAW — build your own IP headers
int raw = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

// AF_PACKET — link-layer access
int pkt = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
```

Raw sockets require `CAP_NET_RAW` (or root). For most packet capture needs, `libpcap` is the better choice.

## ZeroMQ and nanomsg

For serious messaging, consider higher-level abstractions. ZeroMQ provides sockets that handle framing, reconnection, and routing automatically. Nanomsg and nng continue the same philosophy with cleaner code.

```c
// ZeroMQ PUB-SUB (dozens of lines of raw socket code) becomes:
void *ctx = zmq_ctx_new();
void *pub = zmq_socket(ctx, ZMQ_PUB);
zmq_bind(pub, "tcp://*:5556");
zmq_send(pub, "hello", 5, 0);
```

## Summary

| API | Use Case |
|-----|----------|
| `select` | Legacy code, small fd counts |
| `poll` | Better than select, small/medium fd counts |
| `epoll` | Linux high-performance servers |
| `kqueue` | BSD/macOS high-performance servers |
| libevent/libuv | Cross-platform abstraction |
| ZeroMQ | Message-oriented middleware |

Socket programming is deep. Key takeaways:
1. Always handle partial `read()`/`write()`.
2. Use `getaddrinfo()` for DNS.
3. Set `SO_REUSEADDR` on listening sockets.
4. Prefer epoll over select/poll on Linux.
5. Thread-per-connection is simple but doesn't scale beyond ~10K connections.

Next: a deep dive into how system calls actually work.
