# Chapter 5: Signals and Inter-Process Communication

## Signals

A **signal** is a limited form of asynchronous notification sent to a process or thread. Think of them as software interrupts. Signals can be sent by the kernel (e.g., `SIGSEGV` on a bad memory access), by another process (`kill()`), or by the process itself (`raise()`).

### Standard Signals (POSIX)

| Signal | Value | Default Action | Meaning |
|--------|-------|---------------|---------|
| `SIGHUP` | 1 | Terminate | Hangup (terminal closed) |
| `SIGINT` | 2 | Terminate | Interrupt (Ctrl+C) |
| `SIGQUIT` | 3 | Core dump | Quit (Ctrl+\\) |
| `SIGILL` | 4 | Core dump | Illegal instruction |
| `SIGABRT` | 6 | Core dump | abort() |
| `SIGFPE` | 8 | Core dump | Floating point exception |
| `SIGKILL` | 9 | Terminate | Kill (cannot be caught or ignored) |
| `SIGSEGV` | 11 | Core dump | Segmentation fault |
| `SIGPIPE` | 13 | Terminate | Broken pipe (write to closed pipe) |
| `SIGTERM` | 15 | Terminate | Termination request |
| `SIGCHLD` | 17 | Ignore | Child stopped or terminated |
| `SIGCONT` | 18 | Continue | Continue if stopped |
| `SIGSTOP` | 19 | Stop | Stop (cannot be caught or ignored) |
| `SIGUSR1` | 10 | Terminate | User-defined signal 1 |
| `SIGUSR2` | 12 | Terminate | User-defined signal 2 |

Two signals cannot be caught, blocked, or ignored: `SIGKILL` and `SIGSTOP`.

### Sending Signals

```c
#include <signal.h>

kill(pid, SIGTERM);        // send to specific process
killpg(pgrp, SIGINT);      // send to process group
raise(SIGUSR1);            // send to self
```

Despite the name, `kill()` doesn't always kill — it delivers any signal.

```bash
# From the shell:
kill -TERM 1234
kill -USR1 1234
kill -9 1234              # SIGKILL — process has no chance to clean up
```

### Signal Handlers

Install a handler using `signal()` (simple, less portable) or `sigaction()` (recommended):

```c
#include <signal.h>

void handle_sigint(int signo) {
    write(STDOUT_FILENO, "\nCaught SIGINT\n", 15);
    _exit(0);
}

int main(void) {
    struct sigaction sa = {
        .sa_handler = handle_sigint,
        .sa_flags   = SA_RESTART,  // restart interrupted syscalls
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    while (1) pause();  // wait for signal
}
```

**Critical constraint**: Signal handlers run in an async context. Only **async-signal-safe** functions may be called inside a handler. The list includes: `write`, `read`, `open`, `close`, `fork`, `_exit`, `kill`, `signal`, `sigaction`. It **excludes**: `printf`, `malloc`, `fopen`, `fclose`, `pthread_mutex_lock` — anything that might allocate memory or take a lock.

The `volatile sig_atomic_t` pattern for flag-setting:

```c
volatile sig_atomic_t keep_running = 1;

void handle(int sig) { keep_running = 0; }

int main(void) {
    signal(SIGTERM, handle);
    while (keep_running) {
        do_work();
    }
    cleanup();  // graceful shutdown
}
```

### Blocking and Waiting

```c
sigset_t set;
sigemptyset(&set);
sigaddset(&set, SIGINT);
sigprocmask(SIG_BLOCK, &set, NULL);   // block SIGINT

// Critical section — SIGINT deferred
...

sigprocmask(SIG_UNBLOCK, &set, NULL); // pending SIGINT delivered now
```

`sigwait()` atomically unmasks a signal set and waits for one to arrive:

```c
sigset_t set;
sigemptyset(&set);
sigaddset(&set, SIGTERM);
sigaddset(&set, SIGINT);

int sig;
sigwait(&set, &sig);  // clean synchronous wait
printf("Got signal %d\n", sig);
```

`sigwait()` is far cleaner than async handlers for multi-threaded programs — combine it with `pthread_sigmask()`.

## Pipes

A **pipe** is a unidirectional I/O channel between processes, represented by two file descriptors: one for reading, one for writing.

```c
#include <unistd.h>

int pipe(int pipefd[2]);
// pipefd[0] = read end
// pipefd[1] = write end
// returns 0 on success, -1 on error
```

### Parent-Child Communication

```c
int fd[2];
pipe(fd);

pid_t pid = fork();
if (pid == 0) {
    close(fd[1]);              // child closes write end
    char buf[256];
    read(fd[0], buf, sizeof(buf));
    printf("Child received: %s\n", buf);
    close(fd[0]);
    _exit(0);
}

close(fd[0]);                  // parent closes read end
write(fd[1], "hello child", 12);
close(fd[1]);
wait(NULL);
```

Key behaviors:
- `read()` from a pipe with the write end still open blocks until data arrives.
- `read()` from a pipe with **all** write ends closed returns 0 (EOF).
- `write()` to a pipe with **all** read ends closed generates `SIGPIPE` (or returns `EPIPE` if the signal is blocked/ignored).
- Pipes have a kernel buffer (typically 64KB on Linux). Writes smaller than `PIPE_BUF` (4096 bytes) are guaranteed atomic.

### Shell Pipeline Simulation

```c
// Equivalent to: ls | wc -l
int fd[2];
pipe(fd);

if (fork() == 0) {
    dup2(fd[1], STDOUT_FILENO);  // redirect stdout to pipe
    close(fd[0]); close(fd[1]);
    execlp("ls", "ls", NULL);
    _exit(127);
}

if (fork() == 0) {
    dup2(fd[0], STDIN_FILENO);   // redirect stdin from pipe
    close(fd[0]); close(fd[1]);
    execlp("wc", "wc", "-l", NULL);
    _exit(127);
}

close(fd[0]); close(fd[1]);
wait(NULL); wait(NULL);
```

## FIFOs (Named Pipes)

Pipes are anonymous — they exist only while the creating process holds open descriptors. **FIFOs** (or named pipes) have a filesystem entry and persist beyond process lifetimes, enabling communication between unrelated processes.

```bash
mkfifo /tmp/myfifo
```

```c
mkfifo("/tmp/myfifo", 0666);

// Process A (writer)
int fd = open("/tmp/myfifo", O_WRONLY);
write(fd, "data", 4);
close(fd);

// Process B (reader)
int fd = open("/tmp/myfifo", O_RDONLY);
char buf[4];
read(fd, buf, sizeof(buf));
close(fd);
```

FIFOs are great for simple one-to-one IPC but don't scale to multiple readers (reads are atomic only for small messages).

## System V IPC: Message Queues

System V message queues provide typed, structured messaging with the ability to read messages by type (priority-based):

```c
#include <sys/msg.h>

// Create/get queue
int msqid = msgget(ftok("/tmp", 'A'), IPC_CREAT | 0666);

// Send
struct mymsg {
    long mtype;          // message type (> 0)
    char mtext[256];
} msg = { .mtype = 1, .mtext = "Hello" };
msgsnd(msqid, &msg, strlen(msg.mtext) + 1, 0);

// Receive (blocking)
struct mymsg rx;
msgrcv(msqid, &rx, sizeof(rx.mtext), 1, 0);  // type 1 only
// msgrcv(..., -3, 0) — first message with type <= 3
// msgrcv(..., 0, 0)  — first message regardless of type
```

System V IPC objects (message queues, semaphores, shared memory) persist beyond process termination. Clean up with `msgctl(msqid, IPC_RMID, NULL)` or use `ipcrm`.

```bash
ipcs  # list all System V IPC objects
ipcrm -q <msqid>  # remove a queue
```

## System V Shared Memory

```c
#include <sys/shm.h>

int shmid = shmget(ftok("/tmp", 'B'), 4096, IPC_CREAT | 0666);
void *mem = shmat(shmid, NULL, 0);  // attach

// Both processes can now read/write *mem synchronously
sprintf((char*)mem, "shared data");

shmdt(mem);                         // detach
shmctl(shmid, IPC_RMID, NULL);      // destroy
```

No kernel overhead for data transfer — just reads and writes to a shared region. Synchronization must be done manually (semaphores, mutexes, or atomic operations).

## POSIX IPC: A Cleaner Alternative

POSIX provides a newer, cleaner IPC interface with names instead of keys:

```c
// POSIX Message Queues
#include <mqueue.h>
mqd_t mq = mq_open("/myqueue", O_CREAT | O_RDWR, 0666, NULL);
mq_send(mq, "data", 4, 0);
mq_receive(mq, buf, 8192, NULL);
mq_close(mq);
mq_unlink("/myqueue");

// POSIX Shared Memory
#include <sys/mman.h>
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, 4096);
void *mem = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
close(fd);
// ... use mem ...
munmap(mem, 4096);
shm_unlink("/myshm");

// POSIX Semaphores
#include <semaphore.h>
sem_t *sem = sem_open("/mysem", O_CREAT, 0666, 1);
sem_wait(sem);    // P, lock
// critical section
sem_post(sem);    // V, unlock
sem_close(sem);
sem_unlink("/mysem");
```

POSIX IPC is generally preferred for new code over System V — cleaner namespace, ACL-based permissions, and tighter integration with the filesystem.

## Eventfd and Signalfd

Linux-specific mechanisms that turn signals and events into file descriptors, integrating seamlessly with `select`/`poll`/`epoll` loops:

```c
#include <sys/eventfd.h>
#include <sys/signalfd.h>

// eventfd — lightweight event notification between threads/processes
int efd = eventfd(0, EFD_SEMAPHORE);  // semaphore semantics
write(efd, &(uint64_t){1}, 8);        // signal
uint64_t val;
read(efd, &val, 8);                   // wait and consume

// signalfd — receive signals as readable events on a fd
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigprocmask(SIG_BLOCK, &mask, NULL);  // block first
int sfd = signalfd(-1, &mask, 0);

struct signalfd_siginfo fdsi;
read(sfd, &fdsi, sizeof(fdsi));       // reads signal info
```

## Unix Domain Sockets

For local IPC, Unix domain sockets provide the highest bandwidth and lowest latency (the kernel copies data directly, no protocol headers):

```c
#include <sys/un.h>

// Server
int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = "/tmp/mysock" };
unlink("/tmp/mysock");
bind(sfd, (struct sockaddr*)&addr, sizeof(addr));
listen(sfd, 5);
int cfd = accept(sfd, NULL, NULL);
// read/write on cfd...

// Client
int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = "/tmp/mysock" };
connect(cfd, (struct sockaddr*)&addr, sizeof(addr));
// read/write on cfd...
```

Unix sockets support `SOCK_STREAM` (bidirectional byte stream, like TCP), `SOCK_DGRAM` (datagrams, like UDP, but reliable), and `SOCK_SEQPACKET` (record-oriented, reliable). They also support passing file descriptors between processes via `SCM_RIGHTS` ancillary messages — the only portable way to share an open fd.

## Choosing an IPC Mechanism

| Mechanism | Speed | Single/Multi | Persistence | Complexity |
|-----------|-------|-------------|-------------|------------|
| Pipes | Fast | 1:1 | None | Low |
| FIFOs | Fast | N:M | Filesystem | Low |
| Signals | Fast | 1:1 | None | Medium |
| Unix Sockets | Fastest | N:M | Filesystem | Medium |
| POSIX MQ | Medium | N:M | Kernel | Medium |
| Shared Memory | Fastest | N:M | Kernel | High |
| System V IPC | Medium | N:M | Kernel | High |

For most new projects: use **Unix domain sockets** for client-server IPC and **POSIX shared memory + semaphores** for high-throughput pipelines.

Next: threads — shared address spaces and the joys of concurrency.
