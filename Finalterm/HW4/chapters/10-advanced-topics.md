# Chapter 10: Advanced Topics

## Linux Namespaces

Namespaces are the foundation of containerization — they restrict what a process can *see*:

| Namespace | `clone` Flag | Restricts visibility of |
|-----------|-------------|------------------------|
| Mount | `CLONE_NEWNS` | Mount points |
| UTS | `CLONE_NEWUTS` | Hostname, domain name |
| IPC | `CLONE_NEWIPC` | System V IPC, POSIX MQ |
| PID | `CLONE_NEWPID` | Process IDs |
| Network | `CLONE_NEWNET` | Interfaces, routes, iptables |
| User | `CLONE_NEWUSER` | UID/GID mappings |
| Cgroup | `CLONE_NEWCGROUP` | Cgroup view |
| Time | `CLONE_NEWTIME` | Clock offsets (Linux 5.6+) |

### Building a Minimal Container

```c
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/wait.h>

#define STACK_SIZE (1024 * 1024)

static char child_stack[STACK_SIZE];

int child_fn(void *arg) {
    // We're in a new set of namespaces

    // Set hostname (requires CLONE_NEWUTS)
    sethostname("container", 9);

    // Mount /proc (requires CLONE_NEWNS, CLONE_NEWPID)
    mount("proc", "/proc", "proc", 0, NULL);

    // Run a shell
    execl("/bin/bash", "/bin/bash", NULL);
    return 0;
}

int main(void) {
    int flags = SIGCHLD
              | CLONE_NEWNS   // mount namespace
              | CLONE_NEWUTS  // UTS namespace
              | CLONE_NEWPID  // PID namespace
              | CLONE_NEWNET; // network namespace

    pid_t pid = clone(child_fn,
                      child_stack + STACK_SIZE,
                      flags, NULL);

    waitpid(pid, NULL, 0);
    return 0;
}
```

This is essentially what `docker run` does — clone with namespace flags, then set up the root filesystem (pivot_root), cgroups, and capabilities.

## Cgroups (Control Groups)

Namespaces control *visibility*; cgroups control *usage* (resource limits). Cgroups v2 is the current standard:

```bash
# Create a cgroup
mkdir /sys/fs/cgroup/mycontainer

# Limit CPU to 50% of one core
echo "50000 100000" > /sys/fs/cgroup/mycontainer/cpu.max

# Limit memory to 256MB
echo "268435456" > /sys/fs/cgroup/mycontainer/memory.max

# Move a process into the cgroup
echo $PID > /sys/fs/cgroup/mycontainer/cgroup.procs

# Enable IO throttling (requires explicit controller setup)
echo "8:0 wbps=10485760" > /sys/fs/cgroup/mycontainer/io.max
```

Controllers include: `cpu`, `memory`, `io`, `pids`, `cpuset`, `hugetlb`. cgroups v2 uses a unified hierarchy: all controllers are managed under a single tree.

## Capabilities

Traditional Unix has a binary privilege model: you're either root (can do anything) or not. Linux **capabilities** split root's power into distinct, assignable units:

| Capability | Permits |
|-----------|---------|
| `CAP_NET_BIND_SERVICE` | Bind to ports < 1024 |
| `CAP_NET_RAW` | Raw sockets, packet crafting |
| `CAP_SYS_PTRACE` | ptrace any process |
| `CAP_SYS_ADMIN` | Broad admin (mount, swapon, sethostname) |
| `CAP_SYS_TIME` | Set system clock |
| `CAP_SYS_RESOURCE` | Increase resource limits |
| `CAP_SYS_CHROOT` | Use chroot() |
| `CAP_KILL` | Bypass signal permission checks |

```c
#include <sys/capability.h>

// Drop all capabilities
capng_clear(CAPNG_SELECT_BOTH);
capng_apply(CAPNG_SELECT_BOTH);

// Keep only CAP_NET_BIND_SERVICE
capng_clear(CAPNG_SELECT_BOTH);
capng_update(CAPNG_ADD, CAPNG_EFFECTIVE|CAPNG_PERMITTED, CAP_NET_BIND_SERVICE);
capng_apply(CAPNG_SELECT_BOTH);

prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);  // prevent privilege escalation

execl("/usr/bin/server", "server", NULL);
```

Best practice: run services with the *minimum* required capabilities. Drop to a non-root user after binding privileged ports. Set `PR_SET_NO_NEW_PRIVS` to prevent execv'ing setuid binaries.

## seccomp: Syscall Filtering

seccomp (secure computing mode) restricts which syscalls a process can make. In strict mode (deprecated), only `read`, `write`, `_exit`, and `sigreturn` are allowed. Modern software uses **seccomp-bpf** — Berkeley Packet Filter programs that make fine-grained allow/deny decisions per syscall:

```c
#include <linux/seccomp.h>
#include <linux/filter.h>

// Allow write, read, exit, sigreturn — kill on anything else
struct sock_filter filter[] = {
    // Load syscall arch and syscall number
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, arch)),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),

    BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_write, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_read, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_exit, 0, 1),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
};

struct sock_fprog prog = {
    .len = sizeof(filter) / sizeof(filter[0]),
    .filter = filter,
};

prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
```

In practice, use `libseccomp` for a declarative API:

```c
scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(mount), 0);
seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(ptrace), 0);
seccomp_load(ctx);
```

Docker defaults to a whitelist of ~300 syscalls. Chrome sandboxes its renderers to ~50. `systemd` can generate seccomp profiles from unit files:

```ini
[Service]
SystemCallFilter=@system-service
# Blocks ~150 rarely-used syscalls that shouldn't appear in well-behaved services
```

## Namespace + Cgroup + Capabilities + seccomp = Container

Containers are not magic — they are the composition of these four Linux kernel features:

```
Container Security Stack:
┌────────────────────────────────────┐
│ seccomp-bpf    syscall whitelist   │  Layer 4: Attack surface reduction
├────────────────────────────────────┤
│ capabilities   fine-grained privs │  Layer 3: Privilege limitation
├────────────────────────────────────┤
│ namespaces     view isolation     │  Layer 2: Visibility isolation
├────────────────────────────────────┤
│ cgroups        resource limits    │  Layer 1: DoS prevention
└────────────────────────────────────┘
```

Understanding each layer lets you build custom container runtimes, debug "permission denied" errors inside containers, and reason about security boundaries.

## eBPF: Programmable Kernel

eBPF extends the kernel's behavior at runtime with safe, sandboxed programs. Use cases:

- **Networking**: XDP drops DDoS packets at the NIC driver level (100x faster than iptables).
- **Observability**: `bpftrace` one-liners for tracing any kernel function.
- **Security**: Falco detects anomalous behavior with eBPF probes.
- **Performance**: `bcc` tools (execsnoop, opensnoop, tcplife) reveal system activity.

Basic eBPF flow:
1. Write eBPF program (restricted C).
2. Compile to BPF bytecode with Clang.
3. Load into kernel via `bpf()` syscall.
4. Kernel verifier checks safety (no loops, bounded execution, no out-of-bounds).
5. JIT compile to native instructions.
6. Attach to hook point (kprobe, tracepoint, XDP, etc.).
7. Userspace reads output via BPF maps (hash tables, arrays, ring buffers).

```c
// bpftrace one-liners
bpftrace -e 'kprobe:do_sys_open { printf("%s: %s\n", comm, str(arg1)); }'
bpftrace -e 'tracepoint:syscalls:sys_enter_clone { @[comm] = count(); }'
bpftrace -e 'kretprobe:vfs_read { @bytes = hist(retval); }'
```

## NUMA Architecture

Large servers split CPUs and memory into **NUMA nodes** — local memory is fast, remote memory is slow (1.5-3x latency). The kernel is NUMA-aware but needs hints for optimal placement:

```bash
numactl --hardware              # show NUMA topology
numactl --cpubind=0 ./program  # run only on node 0
numactl --membind=0 ./program  # allocate memory only from node 0
numactl --interleave=all ./program  # spread memory across nodes

numastat                         # per-node memory statistics
```

In C:
```c
#include <numaif.h>
#include <numa.h>

// Pin thread to node 0
struct bitmask *mask = numa_parse_nodestring("0");
numa_run_on_node(0);

// Memory policy
mbind(addr, len, MPOL_BIND, mask->maskp, mask->size, 0);
numa_free_nodemask(mask);
```

Performance-critical applications (databases, in-memory stores) are explicitly NUMA-aware — they allocate thread-local heaps on local memory, partition data by NUMA node, and measure cross-node access with `perf`.

## Huge Pages (continued)

For large-memory applications, huge pages reduce TLB pressure:

```bash
# Reserve 1024 huge pages (each 2MB = 2GB total)
echo 1024 > /proc/sys/vm/nr_hugepages

# Use with mmap (MAP_HUGETLB)
# Or transparent huge pages (automatic):
echo always > /sys/kernel/mm/transparent_hugepage/enabled
```

1GB pages:
```bash
# Reserve 1GB pages
echo 4 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages

# Mount hugetlbfs for 1GB pages
mkdir /mnt/huge1g
mount -t hugetlbfs -o pagesize=1GB none /mnt/huge1g
```

## DPDK and Kernel Bypass

For extreme network performance (>10M packets/sec), the kernel becomes a bottleneck. **DPDK (Data Plane Development Kit)** bypasses the kernel entirely — a userspace application takes over the NIC via `uio_pci_generic` or `vfio-pci`, polls for packets directly, and manages its own memory.

Trade-offs: you lose TCP/IP (must implement your own stack or use a library), lose kernel security, and the CPU core spinning at 100% can do nothing else.

DPDK is used in high-frequency trading, 5G infrastructure, and software load balancers.

## The Linux Kernel Module

For the brave: load your own code into the kernel. A minimal module:

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Hello World Module");

static int __init hello_init(void) {
    printk(KERN_INFO "Hello, kernel!\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye, kernel!\n");
}

module_init(hello_init);
module_exit(hello_exit);
```

Build with a kernel makefile:
```makefile
obj-m += hello.o

all:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

```bash
insmod hello.ko    # insert module
lsmod | grep hello # list loaded
dmesg | tail       # see printk output
rmmod hello        # remove module
```

Kernel modules run with full kernel privilege — a bug crashes the entire system. This is not for production experimentation.

## Building a Daemon: Best Practices

Every system programmer will write a daemon. Here is the checklist:

1. **Fork and detach** — `fork()`, parent exits, child calls `setsid()`.
2. **Double fork** — ensures the process is not a session leader and cannot reacquire a controlling terminal.
3. **Change directory** to `/` — avoids locking a mount point.
4. **Set umask** to `0` — explicitly control file permissions.
5. **Close all file descriptors** — except those you need, reopen stdin/out/err to `/dev/null`.
6. **Signal handling** — `SIGTERM` for graceful shutdown, `SIGHUP` for config reload.
7. **PID file** — atomically create `/var/run/mydaemon.pid` with `O_EXCL` to prevent duplicate instances.
8. **syslog** — use `syslog(3)` for structured logging integrated with systemd-journald.
9. **Privilege drop** — start as root for `bind()`, then `setuid()` to an unprivileged user.
10. **Resource limits** — `setrlimit(RLIMIT_NOFILE, ...)` and `RLIMIT_CORE` to control resource usage.

## Summary

System programming is a deep discipline spanning hardware, kernel, and userspace. This book has covered:

- **Processes**: `fork`, `exec`, `wait`, daemons.
- **I/O**: File descriptors, buffered vs unbuffered, mmap, epoll.
- **Memory**: Virtual memory, mmap, malloc internals, huge pages.
- **IPC**: Signals, pipes, FIFOs, message queues, shared memory, Unix sockets.
- **Threads**: pthreads, mutexes, atomics, deadlock prevention.
- **Network**: Sockets, TCP/UDP, epoll, concurrent server architectures.
- **Syscalls**: Mechanics, strace, ptrace, io_uring.
- **Toolchain**: GCC, make/cmake, GDB, perf, sanitizers.
- **Advanced**: Namespaces, cgroups, capabilities, seccomp, eBPF.

The unifying thread is *control* — system programming is about understanding and leveraging the kernel's abstractions to build fast, secure, and reliable software.

### Further Reading

- *The Linux Programming Interface* — Michael Kerrisk (the bible).
- *Advanced Programming in the UNIX Environment* — W. Richard Stevens (classic).
- *Understanding the Linux Kernel* — Bovet & Cesati (kernel internals).
- *Linux Kernel Development* — Robert Love (practical kernel programming).
- *BPF Performance Tools* — Brendan Gregg (eBPF observability).
- Linux man-pages: `man 2` (syscalls), `man 3` (libc), `man 7` (overviews).

Happy hacking. May your syscalls return zero and your segfaults be few.
