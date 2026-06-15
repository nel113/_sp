# Summary

* [Chapter 1: Introduction to System Programming](chapters/01-introduction.md)
  * What Is System Programming?
  * The Role of the Operating System
  * Why C?
  * The System Call Interface
  * Errors and errno
  * Compilation and Linking
  * Static vs. Dynamic Linking
  * Your First System Program
* [Chapter 2: Processes](chapters/02-processes.md)
  * Process IDs
  * fork()
  * exec() Family
  * Fork + Exec = Spawn
  * Process Termination and Zombies
  * Orphan Processes
  * Process State Transitions
  * Process Groups and Sessions
  * Daemonization
  * /proc Filesystem
* [Chapter 3: File Systems and I/O](chapters/03-filesystems-io.md)
  * Everything Is a File
  * File Descriptors
  * Opening and Closing Files
  * Reading and Writing
  * Seeking
  * Buffered I/O (stdio) vs. Unbuffered I/O
  * File Descriptor vs. File Description
  * Scatter-Gather I/O
  * Memory-Mapped I/O
  * File Metadata
  * Directories
  * Asynchronous I/O
* [Chapter 4: Memory Management](chapters/04-memory-management.md)
  * Virtual Memory
  * Pages and Paging
  * brk/sbrk Interface
  * malloc Internals
  * mmap for Memory Allocation
  * mprotect and Memory Protection
  * mlock: Keeping Memory Resident
  * NUMA Awareness
  * Huge Pages
  * Debugging Memory
* [Chapter 5: Signals and IPC](chapters/05-signals-ipc.md)
  * Signals
  * Pipes
  * FIFOs (Named Pipes)
  * System V IPC: Message Queues
  * System V Shared Memory
  * POSIX IPC
  * Eventfd and Signalfd
  * Unix Domain Sockets
  * Choosing an IPC Mechanism
* [Chapter 6: Threads and Concurrency](chapters/06-threads-concurrency.md)
  * Why Threads?
  * POSIX Threads (pthreads)
  * Race Conditions
  * Mutexes
  * Condition Variables
  * Read-Write Locks
  * Atomic Operations
  * Thread-Local Storage
  * Deadlock
  * Thread Pools
  * Futex
  * Spinlocks
  * Barriers
* [Chapter 7: Network Programming](chapters/07-network-programming.md)
  * Berkeley Sockets API
  * TCP Echo Server
  * TCP Client
  * Byte Ordering
  * Non-blocking I/O
  * I/O Multiplexing (select, poll, epoll, kqueue)
  * Concurrent Server Architectures
  * Socket Options
  * getaddrinfo
  * sendfile
  * Raw Sockets
* [Chapter 8: System Calls Deep Dive](chapters/08-system-calls-deep-dive.md)
  * The Boundary Between Worlds
  * x86-64 Syscall Mechanics
  * The System Call Table
  * Wrappers vs Syscalls
  * clone() — The Real Process Primitive
  * vDSO
  * strace
  * ptrace
  * io_uring
  * BPF and Probing Syscalls
* [Chapter 9: Building System Software](chapters/09-building-system-software.md)
  * The Toolchain
  * GCC and Clang
  * Compilation Stages
  * ELF Format
  * Static vs Dynamic Libraries
  * Make
  * CMake
  * Debugging with GDB
  * Profiling with perf
  * Valgrind and Sanitizers
  * Core Dumps
* [Chapter 10: Advanced Topics](chapters/10-advanced-topics.md)
  * Linux Namespaces
  * Cgroups
  * Capabilities
  * seccomp
  * Container Security Stack
  * eBPF
  * NUMA Architecture
  * Huge Pages
  * DPDK and Kernel Bypass
  * Linux Kernel Modules
  * Building a Daemon Checklist
