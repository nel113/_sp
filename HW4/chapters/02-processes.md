# Chapter 2: Processes

A **process** is an instance of a running program. It encapsulates a virtual address space, one or more threads, open file descriptors, signal handlers, and kernel resources like scheduling priority and credentials. The kernel maintains a `task_struct` for every process — representing it in the scheduler's runqueue, tracking its memory mappings, and recording its parent-children relationships.

## Process IDs

Every process has a unique **process ID (PID)** assigned by the kernel. The first process, `init` (PID 1), is spawned by the kernel during boot. All other processes descend from it.

```c
#include <unistd.h>
#include <stdio.h>

int main(void) {
    printf("My PID:   %d\n", getpid());
    printf("Parent:   %d\n", getppid());
    return 0;
}
```

PIDs are 32-bit integers that wrap around. The maximum PID on Linux is `/proc/sys/kernel/pid_max` (default 32768). Once a PID is recycled, the old process must be fully reaped.

## The fork() System Call

`fork()` creates a new process by duplicating the calling process. After the call, two processes execute the same code — the parent and the child. They are nearly identical: same code, same open fds, same memory. The difference is the return value:

- **Parent**: `fork()` returns the child's PID (> 0).
- **Child**: `fork()` returns 0.
- **Error**: returns -1.

```c
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child
        printf("Child:  PID=%d  Parent=%d\n", getpid(), getppid());
    } else {
        // Parent
        printf("Parent: PID=%d  Child=%d\n", getpid(), pid);
        wait(NULL);  // reap the child
    }
    return 0;
}
```

Under the hood, `fork()` uses **copy-on-write (COW)**. Instead of copying all memory pages immediately, the kernel marks pages as read-only in both processes. Only when either process writes to a page does the kernel copy it. This makes `fork()` cheap for large processes that mostly `exec()` right away.

## The exec() Family

While `fork()` duplicates, `exec()` **replaces** the current process image with a new program. There are six variants, differentiated by how they receive arguments and environment:

```c
int execl (const char *path, const char *arg0, ..., NULL);
int execv (const char *path, char *const argv[]);
int execle(const char *path, const char *arg0, ..., NULL, char *const envp[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int execlp(const char *file, const char *arg0, ..., NULL);
int execvp(const char *file, char *const argv[]);
```

Mnemonic: **l** = list (args passed as separate parameters), **v** = vector (args passed as array), **e** = environment, **p** = search `$PATH`.

```c
#include <unistd.h>
#include <stdio.h>

int main(void) {
    char *args[] = {"/bin/ls", "-la", "/tmp", NULL};
    char *envp[] = {"HOME=/root", "PATH=/bin:/usr/bin", NULL};

    execve("/bin/ls", args, envp);

    // execve returns only on error
    perror("execve");
    return 1;
}
```

`execve` is the only true system call; the other five are libc wrappers.

## Fork + Exec = Spawn

The canonical pattern for launching a new program:

```c
pid_t pid = fork();
if (pid == 0) {
    // Child: redirect fds, set groups, then exec
    close(0); open("/dev/null", O_RDONLY);       // stdin
    close(1); open("output.log", O_WRONLY|O_CREAT, 0644); // stdout
    execvp("myprogram", myargs);
    _exit(127);  // never use exit() in child after fork
}
// Parent continues
waitpid(pid, &status, 0);
```

Why `_exit()` instead of `exit()`? `exit()` calls `atexit` handlers and flushes stdio buffers — duplicating the parent's cleanup and potentially corrupting shared files.

## Process Termination and Zombies

A process terminates by calling `exit()`, returning from `main()`, or receiving a fatal signal. The kernel releases most resources immediately — memory mappings, open fds, timer events. But the `task_struct` and PID are retained until the parent calls `wait()`.

This half-dead process is a **zombie**. A zombie cannot be killed (it is already dead); the parent must reap it. If the parent dies first, `init` (PID 1) adopts orphans and reaps them automatically.

```c
#include <sys/wait.h>

pid_t pid = fork();
if (pid == 0) {
    _exit(42);  // child exits immediately
}

int status;
pid_t w = waitpid(pid, &status, 0);

if (WIFEXITED(status))
    printf("Child %d exited with %d\n", w, WEXITSTATUS(status));
else if (WIFSIGNALED(status))
    printf("Child %d killed by signal %d\n", w, WTERMSIG(status));
```

The `status` integer encodes both the exit code and the termination reason. Use the macros: `WIFEXITED`, `WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`, `WCOREDUMP`, `WIFSTOPPED`, `WSTOPSIG`.

## Orphan Processes

When a parent dies before its child, the child becomes an **orphan** and is reparented to `init` (PID 1). The `init` process periodically calls `wait()` to clean up orphans. This is why daemons are often double-forked — the intermediate parent exits, making `init` the grandparent responsible for reaping.

## Process State Transitions

```
               +------+
    created -->| READY|--+
               +------+  |scheduled
                    ^     v
                    |  +---------+
                    +--| RUNNING |--> terminated
                       +---------+
                           |blocked on I/O
                           v
                       +--------+
                       |BLOCKED |
                       +--------+
                           |I/O complete
                           v
                        (READY)
```

The kernel's Completely Fair Scheduler (CFS) allocates CPU time slices proportional to process weight (nice value). Interactive processes get a slight priority boost to reduce latency.

## Process Groups and Sessions

A **process group** is a set of related processes that can receive signals as a unit (e.g., Ctrl+C sends `SIGINT` to the foreground group). Every process belongs to exactly one group.

A **session** is a collection of process groups, typically associated with a controlling terminal. The session leader (usually the shell) sets up the terminal.

```c
pid_t pgid = getpgid(0);       // get my group
pid_t sid  = getsid(0);         // get my session
setpgid(0, 0);                  // create new group
setsid();                       // create new session (for daemons)
```

Daemons call `setsid()` to detach from the controlling terminal and protect themselves from terminal-generated signals.

Here is the classic daemonization sequence:

```c
void daemonize(void) {
    pid_t pid = fork();
    if (pid > 0) _exit(0);               // parent exits
    if (pid == -1) { perror("fork"); _exit(1); }

    setsid();                              // new session, no terminal

    pid = fork();                          // double-fork: grandchild
    if (pid > 0) _exit(0);                // is never session leader
    if (pid == -1) { perror("fork"); _exit(1); }

    chdir("/");                            // avoid locking mount points
    umask(0);                              // full permission control

    for (int fd = 0; fd < sysconf(_SC_OPEN_MAX); fd++)
        close(fd);                         // close all inherited fds

    // reopen stdin/stdout/stderr to /dev/null
    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    if (devnull > 2) close(devnull);
}
```

## /proc Filesystem

Linux exposes process information through the `/proc` pseudo-filesystem. `/proc/<PID>/` contains:

| File | Contents |
|------|----------|
| `cmdline` | Command-line arguments |
| `environ` | Environment variables |
| `fd/` | Open file descriptors (symlinks to files) |
| `maps` | Memory mappings |
| `status` | Human-readable process status |
| `stat` | Machine-parsable process info |
| `limits` | Resource limits |
| `cgroup` | Control group membership |
| `ns/` | Namespace identifiers |

```bash
cat /proc/self/maps        # memory map of `cat` itself
ls -l /proc/$$/fd          # file descriptors of current shell
```

## Summary

Processes are the fundamental unit of execution. Master `fork()` + `exec()` + `wait()` and you master Unix. The next chapter dives into file I/O — how processes communicate with the outside world through file descriptors.
