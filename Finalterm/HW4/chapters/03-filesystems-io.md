# Chapter 3: File Systems and I/O

## Everything Is a File

One of Unix's most powerful design decisions is the **"everything is a file"** abstraction. Regular files on disk, pipes, sockets, terminals, and even hardware devices are accessed through a uniform interface: **file descriptors** plus `read()` and `write()`.

This uniformity means a program that processes text from stdin works equally well with keyboard input, a file, a network socket, or the output of another program.

## File Descriptors

A **file descriptor** is a small non-negative integer that identifies an open file within a process. Each process maintains a per-process file descriptor table in the kernel. The first three descriptors are standard:

| FD | Constant | Stream | Meaning |
|----|----------|--------|---------|
| 0 | `STDIN_FILENO` | stdin | Standard input |
| 1 | `STDOUT_FILENO` | stdout | Standard output |
| 2 | `STDERR_FILENO` | stderr | Standard error |

```c
#include <unistd.h>

char buf[] = "Hello\n";
write(STDOUT_FILENO, buf, sizeof(buf) - 1);  // writes to fd 1
```

## Opening and Closing Files

```c
#include <fcntl.h>
#include <unistd.h>

int fd = open("file.txt", O_RDONLY);           // read-only
int fd = open("file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);  // write, create, truncate
int fd = open("file.txt", O_RDWR | O_APPEND); // read-write, append

if (fd == -1) {
    perror("open");
    exit(EXIT_FAILURE);
}

close(fd);  // free the descriptor for reuse
```

Key flags:
- `O_CREAT` — create file if it doesn't exist (requires mode argument).
- `O_TRUNC` — truncate to zero length on open.
- `O_APPEND` — all writes append to end (atomic in multi-writer scenarios).
- `O_EXCL` — fail if file exists (used with `O_CREAT` for lock files).
- `O_NONBLOCK` — open in non-blocking mode.
- `O_CLOEXEC` — close-on-exec: atomically prevents leaks across fork/exec.
- `O_DIRECT` — bypass the page cache (raw I/O).

## Reading and Writing

```c
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
```

Both return the number of bytes transferred. A `read()` returning fewer bytes than requested is **normal** — it means the kernel had less data available. A `read()` returning 0 means **EOF** (end-of-file). A `write()` returning fewer bytes than requested means the output buffer is full; the caller should retry.

```c
// Robust read loop
ssize_t read_all(int fd, void *buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, (char*)buf + total, n - total);
        if (r == 0) break;          // EOF
        if (r == -1) {
            if (errno == EINTR) continue;  // interrupted by signal
            return -1;              // real error
        }
        total += r;
    }
    return total;
}
```

Always handle `EINTR`. On Linux, a `read()` or `write()` to a slow device (pipe, socket, terminal) can be interrupted by a signal before any data is transferred.

## Seeking

Every open file has a **current offset** that determines where the next `read()` or `write()` starts.

```c
#include <unistd.h>

off_t lseek(int fd, off_t offset, int whence);
```

`whence` values:
- `SEEK_SET` — offset from beginning of file.
- `SEEK_CUR` — offset from current position.
- `SEEK_END` — offset from end of file.

Returns the new offset on success, -1 on error. On Linux, `lseek(fd, 0, SEEK_CUR)` returns the current offset without moving — useful for pipes and sockets that don't support seeking but can report position.

```c
off_t curr = lseek(fd, 0, SEEK_CUR);  // get current position
off_t size = lseek(fd, 0, SEEK_END);  // file size (but use fstat instead!)
```

## Buffered I/O (stdio) vs. Unbuffered I/O

The C standard library provides buffered I/O through `FILE*` streams:

```c
FILE *fp = fopen("file.txt", "r");
char line[256];
fgets(line, sizeof(line), fp);   // buffered
fprintf(fp, "value=%d\n", 42);   // buffered
fclose(fp);                       // flushes buffer
```

Buffered I/O reduces the number of system calls by batching small reads/writes in user-space buffers. This is critical for performance — a system call costs hundreds of CPU cycles.

Modes:
- **Fully buffered**: Block I/O for regular files. Flushed when buffer fills or at `fclose()`.
- **Line buffered**: Flushed on newline. Used for terminal streams.
- **Unbuffered**: Every write is a system call. `stderr` is usually unbuffered.

```c
setvbuf(fp, NULL, _IONBF, 0);     // unbuffered
setvbuf(fp, NULL, _IOLBF, 1024);  // line-buffered, 1KB
setvbuf(fp, NULL, _IOFBF, 8192);  // fully-buffered, 8KB
```

**Rule of thumb**: Use `stdio` for line-oriented text processing. Use raw `read()`/`write()` with your own buffer management for binary data, high-performance applications, and precise error handling.

## File Descriptor vs. File Description

A subtle distinction:

- **File descriptor** (`int fd`): A per-process handle. Two processes have different fds pointing to the same file description.
- **File description** (kernel's `struct file`): Tracks the current offset, access mode, and inode reference. Shared across `dup()` and `fork()`.

```c
int fd2 = dup(fd1);    // fd2 shares the same file description
int fd3 = dup2(fd1, 5); // atomically: close(5), then dup fd1 → 5
```

After `dup(fd)`, both descriptors share the same offset. Writing through one advances the offset for both. After `open()` on the same file twice, you get independent file descriptions with independent offsets.

## Scatter-Gather I/O

`readv()` and `writev()` transfer data to/from multiple non-contiguous buffers in a single system call:

```c
#include <sys/uio.h>

struct iovec {
    void  *iov_base;  // buffer address
    size_t iov_len;   // buffer length
};

ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
```

This is perfect for framing protocols. Write a length header followed by a payload atomically:

```c
struct iovec iov[2];
uint32_t len = htonl(payload_size);
iov[0].iov_base = &len;
iov[0].iov_len  = sizeof(len);
iov[1].iov_base = payload;
iov[1].iov_len  = payload_size;

writev(sockfd, iov, 2);  // single syscall, no header+copy
```

## Memory-Mapped I/O

`mmap()` maps a file (or portion) directly into the process's address space. Reads and writes to the mapped memory region are reflected in the file (and vice versa).

```c
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);

int munmap(void *addr, size_t length);  // unmap
```

```c
int fd = open("data.bin", O_RDONLY);
struct stat st;
fstat(fd, &st);

void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
close(fd);  // mapping persists after close

// Access the file as a byte array
uint32_t *data = (uint32_t*)map;
printf("First word: %u\n", data[0]);

munmap(map, st.st_size);
```

`MAP_PRIVATE` creates a COW mapping — modifications are not written back. `MAP_SHARED` maps are visible to other processes mapping the same file and are used for shared-memory IPC.

Benefits: zero-copy I/O, lazy loading (pages faulted in on demand), and simpler code than `lseek`/`read` loops.

## File Metadata: stat, fstat, lstat

```c
#include <sys/stat.h>

struct stat {
    dev_t     st_dev;      // device
    ino_t     st_ino;      // inode number
    mode_t    st_mode;     // file type and permissions
    nlink_t   st_nlink;    // hard link count
    uid_t     st_uid;      // owner
    gid_t     st_gid;      // group
    dev_t     st_rdev;     // device ID (if special file)
    off_t     st_size;     // total size in bytes
    blksize_t st_blksize;  // preferred I/O block size
    blkcnt_t  st_blocks;   // number of 512B blocks allocated
    struct timespec st_atim; // access time
    struct timespec st_mtim; // modification time
    struct timespec st_ctim; // status change time
};

int stat(const char *pathname, struct stat *st);
int fstat(int fd, struct stat *st);
int lstat(const char *pathname, struct stat *st);  // no symlink follow
```

File type macros: `S_ISREG(m)`, `S_ISDIR(m)`, `S_ISCHR(m)`, `S_ISBLK(m)`, `S_ISFIFO(m)`, `S_ISLNK(m)`, `S_ISSOCK(m)`.

```c
struct stat st;
if (stat("mydir", &st) == 0 && S_ISDIR(st.st_mode))
    printf("It's a directory!\n");
```

## Directories

```c
#include <dirent.h>

DIR *dp = opendir("/home");
struct dirent *entry;
while ((entry = readdir(dp)) != NULL) {
    printf("%s (ino=%lu)\n", entry->d_name, entry->d_ino);
}
closedir(dp);
```

Modern Linux also offers `getdents64()` for lower-level directory reading with bulk transfers, and `openat()` plus `*at()` family calls for directory-relative operations that avoid race conditions with `chdir()`.

## Asynchronous I/O

Linux supports several async I/O models:

1. **libaio** — true kernel AIO (requires `O_DIRECT`, limited use).
2. **io_uring** — modern, efficient, ring-buffer-based I/O (Linux 5.1+).
3. **POSIX AIO** (`aio_read`, `aio_write`) — libc implementation with threads under the hood.

`io_uring` is the future. It uses a shared ring buffer between kernel and user space, eliminating syscall overhead for submission and completion.

## Summary

File I/O is the backbone of system programming. From `open`/`read`/`write` to scatter-gather I/O and memory mapping, the kernel provides a rich set of primitives for moving data. Understanding the distinction between file descriptors and file descriptions, and between buffered and unbuffered I/O, is essential for writing correct and performant systems code.

In the next chapter, we explore virtual memory — how the kernel manages address spaces and how you can bend them to your will.
