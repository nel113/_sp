# The Art of System Programming: From Bits to Processes

An open-source book on Linux system programming — from file descriptors to containers.

## About

This book covers the foundations of system programming on Linux: processes, memory, I/O, networking, threads, signals, build tools, and kernel interfaces. It assumes you are comfortable with C and a Unix command line. Every chapter includes runnable code examples and practical advice.

## Table of Contents

1. [**Introduction to System Programming**](chapters/01-introduction.md) — OS role, system calls, C vs Rust, compilation pipeline, linking.
2. [**Processes**](chapters/02-processes.md) — fork, exec, wait, zombies, orphans, process groups, daemonization.
3. [**File Systems and I/O**](chapters/03-filesystems-io.md) — file descriptors, open/read/write/close, buffered I/O, scatter-gather, mmap, AIO.
4. [**Memory Management**](chapters/04-memory-management.md) — virtual memory, pages, brk/sbrk, malloc internals, mmap, mprotect, mlock, huge pages.
5. [**Signals and IPC**](chapters/05-signals-ipc.md) — signals, pipes, FIFOs, message queues, shared memory, Unix sockets, eventfd.
6. [**Threads and Concurrency**](chapters/06-threads-concurrency.md) — pthreads, mutexes, condition variables, atomics, deadlock, thread pools.
7. [**Network Programming**](chapters/07-network-programming.md) — sockets, TCP/UDP, select/poll/epoll, getaddrinfo, concurrent servers.
8. [**System Calls Deep Dive**](chapters/08-system-calls-deep-dive.md) — syscall mechanics, syscall table, strace, ptrace, io_uring, vDSO, eBPF.
9. [**Building System Software**](chapters/09-building-system-software.md) — GCC/Clang, ELF, static/shared libs, Make/CMake, GDB, perf, sanitizers.
10. [**Advanced Topics**](chapters/10-advanced-topics.md) — Linux namespaces, cgroups, capabilities, seccomp, eBPF, NUMA, DPDK, kernel modules.

## How to Read

- **Linear**: Read chapters 1–10 in order for a complete education.
- **Reference**: Jump to any chapter — each is self-contained.
- **With code**: Every example compiles and runs on Linux (kernel 5.x+, x86-64).

## Contributing

Pull requests are welcome. All code examples should be tested with `gcc -Wall -Wextra -Werror` and `clang` on a recent Linux.

## License

Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0).
See [LICENSE](LICENSE) for details.
