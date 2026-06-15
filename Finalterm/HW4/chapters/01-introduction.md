# Chapter 1: Introduction to System Programming

## What Is System Programming?

**System programming** is the craft of writing software that interfaces directly with the operating system kernel and hardware. Unlike application programming, which consumes abstractions, system programming *builds* them. Every time you `fork()` a process, map memory with `mmap()`, or write bytes to a file descriptor, you are doing system programming.

System programmers create the foundations: operating systems, device drivers, compilers, linkers, debuggers, networking stacks, containers, and hypervisors. The code runs close to the metal — in C, C++, Rust, or assembly — where performance and control matter most.

## The Role of the Operating System

An operating system provides three fundamental abstractions:

1. **Processes** — the illusion that each program owns the entire CPU.
2. **Virtual Memory** — the illusion that each process owns the entire address space.
3. **Files** — a uniform I/O interface for disks, sockets, pipes, and devices.

These abstractions are enforced through **dual-mode operation**: the CPU runs in either *user mode* (restricted) or *kernel mode* (privileged). System calls are the controlled gateways between them.

```
User Space          Kernel Space
+---------+         +------------------+
|  app.c  |--syscall-->| sys_call_table  |
| libc    |         |  filesystem      |
+---------+         |  scheduler       |
                    |  memory mgmt     |
                    +------------------+
```

## Why C?

C remains the lingua franca of system programming because:

- **No runtime**: C programs compile directly to machine code with minimal overhead. No garbage collector, no JIT, no hidden allocations.
- **Pointers**: Direct memory access lets you manipulate hardware registers, craft custom allocators, and memory-map files.
- **ABI stability**: The C calling convention is the universal glue between languages and the kernel.
- **Ubiquity**: The Linux kernel, glibc, Python's CPython interpreter, and PostgreSQL are all written in C.

Rust is emerging as a serious contender with its ownership model that eliminates entire classes of memory bugs at compile time, but C remains the baseline every system programmer must understand.

## The System Call Interface

A **system call** is a function request from user space to the kernel. On x86-64 Linux, the convention is:

1. Load the syscall number into `rax`.
2. Load arguments into `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`.
3. Execute the `syscall` instruction.
4. The return value lands in `rax`.

```c
// glibc wraps this for you:
ssize_t n = write(STDOUT_FILENO, "hello\n", 6);

// Under the hood (amd64):
// mov $1, %rax          // __NR_write is 1
// mov $1, %rdi          // fd = stdout
// mov $msg, %rsi        // buf
// mov $6, %rdx          // count
// syscall
```

Linux exposes over 400 system calls. Common ones include `read`, `write`, `open`, `close`, `fork`, `execve`, `mmap`, `brk`, `ioctl`, `poll`, `socket`, `clone`, and `exit`.

## Errors and errno

System calls return `-1` on failure and set the global `errno` variable. Always check return values:

```c
#include <unistd.h>
#include <errno.h>
#include <string.h>

ssize_t result = read(fd, buf, sizeof(buf));
if (result == -1) {
    fprintf(stderr, "read failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
}
```

The perror(3) function is your friend during development, but production code should handle errors gracefully — retrying on `EINTR`, closing on `EBADF`, and logging `ENOSPC`.

## Compilation and Linking

A C program goes through four stages:

```
source.c → [preprocessor] → translation_unit.i
         → [compiler]      → assembly.s
         → [assembler]     → object.o
         → [linker]        → executable
```

- **Preprocessor** (`-E`): expands `#include`, `#define`, and conditionals.
- **Compiler** (`-S`): translates C to assembly.
- **Assembler** (`-c`): produces the `.o` object file containing machine code and relocations.
- **Linker** (`ld`): resolves symbols across object files and libraries to produce the final binary.

```bash
gcc -E hello.c -o hello.i    # preprocess
gcc -S hello.i -o hello.s    # compile to assembly
gcc -c hello.s -o hello.o    # assemble
gcc hello.o -o hello         # link
```

## Static vs. Dynamic Linking

**Static linking** copies all library code into the executable. The binary is self-contained and portable, but large and hard to update.

**Dynamic linking** defers symbol resolution to load time. The dynamic linker (`ld.so`) maps shared libraries (`.so`) into the process's address space at startup.

```bash
# Static
gcc -static prog.c -o prog

# Dynamic (default)
gcc prog.c -o prog

# Inspect dependencies
ldd ./prog
```

Dynamic linking saves disk and memory (multiple processes share the same library pages), but introduces startup latency and the dreaded "DLL hell" on some platforms.

## Your First System Program

```c
#include <unistd.h>
#include <sys/syscall.h>

// Bypass libc — call write directly via syscall(2)
int main(void) {
    const char msg[] = "Hello, kernel!\n";
    syscall(SYS_write, STDOUT_FILENO, msg, sizeof(msg) - 1);
    syscall(SYS_exit, 0);
}
```

Compile and trace:

```bash
gcc -static raw_write.c -o raw_write
strace ./raw_write
# write(1, "Hello, kernel!\n", 14) = 14
# exit(0)                        = ?
```

## Outline of This Book

| Chapter | Topic |
|---------|-------|
| 2 | Processes — fork, exec, wait |
| 3 | File Systems and I/O |
| 4 | Memory Management |
| 5 | Signals and IPC |
| 6 | Threads and Concurrency |
| 7 | Network Programming |
| 8 | System Calls Deep Dive |
| 9 | Building System Software |
| 10 | Advanced Topics |

This book assumes you are comfortable with C, a text editor, and a Unix command line. All examples target Linux (kernel 5.x+ on x86-64) and are tested with GCC and Clang. The concepts generalize to other Unix-like systems — FreeBSD, macOS, and even the Windows Subsystem for Linux.

Let's begin.
