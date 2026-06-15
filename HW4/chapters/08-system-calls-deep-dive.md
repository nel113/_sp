# Chapter 8: System Calls Deep Dive

## The Boundary Between Worlds

A system call is the controlled mechanism by which user-space code requests kernel services. It is the only legitimate way to cross from user mode (ring 3 on x86) to kernel mode (ring 0).

Why is this necessary? The CPU enforces privilege separation:
- Ring 3 cannot execute privileged instructions (HLT, IN/OUT, setting CR3).
- Ring 3 cannot access kernel memory (page tables mark it as supervisor-only).
- Ring 3 cannot directly call kernel functions (addresses are hidden).

The system call bridges this gap through a specific CPU instruction that performs both a privilege escalation and a controlled jump.

## x86-64 Syscall Mechanics

On x86-64 Linux, the canonical syscall sequence uses the `syscall` instruction (Intel) or equivalent `sysenter` (AMD):

```asm
; write(1, "hello\n", 6)
mov rax, 1          ; __NR_write
mov rdi, 1          ; fd = stdout
mov rsi, msg        ; buf
mov rdx, 6          ; count
syscall             ; enter kernel
; return value in rax
```

The `syscall` instruction does several things atomically:
1. Saves `RIP` (return address) into `RCX`.
2. Saves `RFLAGS` into `R11`.
3. Loads `CS` and `SS` selectors from `MSR_STAR` — switching to ring 0.
4. Loads the kernel entry point from `MSR_LSTAR` (`entry_SYSCALL_64`).
5. Sets `IF=0` (mask interrupts) inside the handler via `rflags`.

The kernel entry point `entry_SYSCALL_64` (defined in `arch/x86/entry/entry_64.S`) saves all general-purpose registers, switches to the kernel stack, calls the actual syscall handler through the `sys_call_table`, then restores registers and executes `sysretq` to return to userspace.

## The System Call Table

The kernel maintains a table (`sys_call_table`) mapping syscall numbers to handler functions. For x86-64, it's defined in `arch/x86/entry/syscalls/syscall_64.tbl`:

```
0   common  read            sys_read
1   common  write           sys_write
2   common  open            sys_open
3   common  close           sys_close
...
59  common  execve          sys_execve
60  common  exit            sys_exit
...
```

There are ~450 system calls on modern Linux. You can see your kernel's list:

```bash
man 2 syscalls                    # manual
grep __NR /usr/include/asm/unistd_64.h  # C header
cat /proc/kallsyms | grep sys_call_table
```

## Wrappers Are Not the Syscall

glibc wraps most syscalls in functions that handle errno, cancellation points, and platform quirks:

```c
// What you write:
write(fd, buf, len);

// Approximate glibc internals:
ssize_t __libc_write(int fd, const void *buf, size_t len) {
    // thread cancellation check here
    ssize_t result = SYSCALL_CANCEL(write, fd, buf, len);
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return result;
}
```

Some syscalls have no glibc wrapper (like `gettid`, `gettid`). You call them through `syscall()`:

```c
#include <sys/syscall.h>
#include <unistd.h>

pid_t tid = syscall(SYS_gettid);  // direct syscall invocation
```

And a few syscalls exist only as glibc wrappers — `fork()` is actually the `clone` syscall with specific flags.

## clone() — The Real Process Primitive

Both `fork()` and `pthread_create()` use the same underlying syscall: `clone(2)`. The flags determine what's shared:

```c
#include <sched.h>

// Equivalent to fork()
clone(child_func, child_stack, SIGCHLD, NULL);

// Equivalent to pthread_create()
clone(child_func, child_stack,
      CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
      CLONE_THREAD | CLONE_SYSVSEM, NULL);
```

Flags:
| Flag | Shares |
|------|--------|
| `CLONE_VM` | Address space (thread) |
| `CLONE_FS` | Filesystem info (cwd, root, umask) |
| `CLONE_FILES` | File descriptor table |
| `CLONE_SIGHAND` | Signal handlers |
| `CLONE_THREAD` | Thread group (same PID) |
| `CLONE_VFORK` | Parent blocks until child execs/exits |
| `CLONE_NEWNS` | New mount namespace |
| `CLONE_NEWUTS` | New hostname/domain name namespace |
| `CLONE_NEWIPC` | New IPC namespace |
| `CLONE_NEWPID` | New PID namespace |
| `CLONE_NEWNET` | New network namespace |

Containers are built from `clone()` with namespace flags — this is how Docker isolates processes.

## vDSO: Syscalls Without Syscalls

Some "syscalls" are actually implemented in userspace through the **vDSO (Virtual Dynamically-linked Shared Object)** — a small shared library the kernel maps into every process's address space.

```bash
cat /proc/self/maps | grep vdso
# 7ffd8c9f8000-7ffd8c9fa000 r-xp 00000000 00:00 0  [vdso]
```

Functions typically implemented in vDSO:
- `gettimeofday()` — read kernel-maintained time from a shared memory page.
- `clock_gettime()` — most clocks are vDSO-accelerated.
- `time()` — seconds since epoch.
- `getcpu()` — current CPU number (avoids `sched_getcpu()` syscall).

The vDSO works by the kernel mapping a read-only page containing time values (updated by the kernel's timer interrupt) into userspace. The vDSO function reads these directly — no context switch. This is why `clock_gettime()` is ~20ns on modern hardware.

```c
#include <sys/auxv.h>

// Get vDSO function pointers for manual use
unsigned long vdso = getauxval(AT_SYSINFO_EHDR);
```

## strace: Observing Syscalls

`strace` traces all system calls made by a process:

```bash
strace ./myprogram
strace -e trace=open,read,write ./myprogram    # filter
strace -c ./myprogram                           # count + timing summary
strace -p 1234                                  # attach to running process
strace -f ./myprogram                           # follow children (-ff for per-thread files)
```

Example output:
```
write(1, "Hello\n", 6)                  = 6
openat(AT_FDCWD, "/etc/passwd", O_RDONLY) = 3
read(3, "root:x:0:0:root:/root:/bin/bash\n"..., 4096) = 1766
close(3)                                = 0
exit_group(0)                           = ?
```

`strace` is indispensable for debugging permission issues, understanding I/O patterns, and reverse-engineering closed-source binaries.

## ptrace: The Debugger's Engine

`ptrace` is the syscall behind `strace`, `gdb`, and `lldb`. It lets a tracer process inspect and control a tracee:

```c
#include <sys/ptrace.h>

// Basic ptrace operations:
ptrace(PTRACE_ATTACH, pid, NULL, NULL);     // attach
ptrace(PTRACE_SYSCALL, pid, NULL, NULL);    // stop at next syscall
ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL); // single instruction
ptrace(PTRACE_GETREGS, pid, NULL, &regs);   // read registers
ptrace(PTRACE_PEEKDATA, pid, addr, NULL);   // read memory
ptrace(PTRACE_CONT, pid, NULL, NULL);       // resume
ptrace(PTRACE_DETACH, pid, NULL, NULL);     // detach
```

A minimal `strace` clone:

```c
pid_t pid = fork();
if (pid == 0) {
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    raise(SIGSTOP);  // wait for tracer
    execvp(argv[1], &argv[1]);
    _exit(127);
}

wait(NULL);
ptrace(PTRACE_SETOPTIONS, pid, NULL, PTRACE_O_TRACESYSGOOD);

while (1) {
    ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
    wait(NULL);

    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, NULL, &regs);
    printf("syscall: %lld\n", regs.orig_rax);

    ptrace(PTRACE_SYSCALL, pid, NULL, NULL);  // trace exit
    wait(NULL);
}
```

## io_uring: The Future of Async Syscalls

`io_uring` (Linux 5.1+) redefines the syscall interface for high-throughput I/O using shared ring buffers:

```
Userspace                          Kernel
+------------+                     +------------+
| Submission | --squeue-> write --> |            |
| Queue      |                     |  io_uring  |
| (SQ)       | <-cqueue-- read --- |  worker    |
+------------+                     +------------+
        |                                |
        `-- mmap'd shared memory --------'
```

```c
#include <linux/io_uring.h>
#include <liburing.h>  // liburing helper library

struct io_uring ring;
io_uring_queue_init(256, &ring, 0);  // 256-entry SQ

struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
io_uring_prep_write(sqe, fd, buf, len, offset);
io_uring_submit(&ring);

struct io_uring_cqe *cqe;
io_uring_wait_cqe(&ring, &cqe);
int result = cqe->res;
io_uring_cqe_seen(&ring, cqe);
```

Key advantages:
- **Zero-syscall submission**: If the SQ has space, `io_uring_enter()` can be avoided (via `IORING_SETUP_SQPOLL`).
- **Batched completions**: Multiple I/Os can complete in a single loop iteration.
- **Fixed buffers/files**: Pre-register buffers to avoid repeated kernel allocations.
- **Any operation**: Read, write, accept, connect, fsync, splice, and more — all unified.

io_uring is the high-performance I/O engine for NVMe, 100Gbps networking, and databases like ScyllaDB.

## BPF and Probing Syscalls

**eBPF (Extended Berkeley Packet Filter)** allows injecting safe sandboxed programs into the kernel to observe and modify system behavior:

```bash
# Count syscalls by PID
bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[comm] = count(); }'

# Trace open() calls
bpftrace -e 'tracepoint:syscalls:sys_enter_openat {
    printf("%s openat(\"%s\")\n", comm, str(args->filename));
}'
```

eBPF programs are compiled to bytecode, verified for safety (no loops, bounded execution), and JIT-compiled to native code. They run in response to kprobes, tracepoints, uprobes, or socket events. This is the foundation of observability tools (BCC, bpftrace, Cilium, Falco).

## Summary

| Tool | Purpose |
|------|---------|
| `strace` | Trace syscalls (debugging) |
| `ptrace` | Process control (debuggers) |
| `perf` | Hardware/software performance counters |
| `ftrace` | Kernel function tracing |
| `bpftrace` | Dynamic kernel instrumentation |
| `io_uring` | High-performance async I/O |

System calls are the narrow waist of the operating system — everything above and below must conform to this interface. Understanding their cost, semantics, and instrumentation tools separates a developer from a systems engineer.

Next: toolchain and build systems — bridging source code to running processes.
