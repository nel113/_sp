# Chapter 4: Memory Management

## Virtual Memory

**Virtual memory** is the fundamental abstraction that gives each process the illusion of owning the entire address space. The kernel, with hardware help from the **MMU (Memory Management Unit)**, translates virtual addresses to physical addresses through page tables.

```
Virtual Address              Physical Memory
+-----------------+          +-----+
| stack           |          |     |
+-----------------+          +-----+
|       |         |          |     |
|       v         |          +-----+
|                 |          |     |
|  (unmapped)     |          +-----+
|                 |          |     |
|       ^         |          +-----+
|       |         |          |     |
+-----------------+          +-----+
| heap (brk/sbrk)|
+-----------------+
| BSS (zero-init) |
+-----------------+
| data (init'd)   |
+-----------------+
| text (code)     |
+-----------------+ 0x0
    ^-- reserved (NULL guard)
```

This layout is the classic Unix address space. On 64-bit Linux, the user space occupies the lower 128 TB (0x0 to 0x00007FFFFFFFFFFF), and kernel space the upper 128 TB.

## Pages and Paging

Memory is managed in fixed-size units called **pages** (typically 4096 bytes). The kernel tracks each page with a `struct page`. When a process accesses a virtual address that has no physical page mapped, the MMU triggers a **page fault**, and the kernel handles it by:

1. Finding a free physical page.
2. If the data is on disk (e.g., an mmap'd file), reading it in.
3. Updating the page table.
4. Restarting the faulting instruction.

This **demand paging** means the kernel never loads what isn't needed.

```bash
getconf PAGE_SIZE   # 4096 on x86-64
```

## The brk/sbrk Interface

`brk` and `sbrk` adjust the program break — the address just past the BSS segment, marking the end of the heap:

```c
#include <unistd.h>

int    brk(void *addr);                      // set break to addr
void  *sbrk(intptr_t increment);             // move break by increment
```

These are low-level calls. The libc `malloc()` uses `brk` for small allocations and `mmap()` for large ones (typically >128KB, controlled by `M_MMAP_THRESHOLD`).

```c
// Raw brk usage (don't do this in production)
void *initial = sbrk(0);           // current break
void *ptr = sbrk(4096);            // extend heap by 4KB
brk(initial);                      // shrink back
```

## malloc Internals

`malloc()` isn't a system call — it's a user-space memory allocator in libc. Understanding its internals helps you write allocation-friendly code.

Modern glibc uses **ptmalloc**, a derivative of Doug Lea's dlmalloc with per-thread arenas to reduce lock contention.

### Allocation Sizes

- **Small** (< size threshold): Satisfied from freelists within an arena. Uses chunk coalescing to merge adjacent free blocks.
- **Large**: Uses `mmap()` directly, returning memory to the OS on `free()`.

### Fragmentation

- **Internal fragmentation**: Wasted space inside an allocated block due to rounding up to alignment.
- **External fragmentation**: Free memory split into small non-contiguous chunks, failing large allocations despite plenty of total free space.

### Best Practices

1. Allocate once, reuse: avoid malloc/free in hot loops.
2. Use `calloc(n, size)` for arrays — it checks for overflow and zeroes memory (zeroing is COW-optimized).
3. Use `realloc()` to grow buffers rather than malloc+copy+free.
4. Prefer stack allocation for small, short-lived objects.
5. Consider slab allocators or object pools for fixed-size allocations.

```c
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);  // overflow-safe
```

## mmap for Memory Allocation

Beyond file mapping, `mmap()` is used for anonymous memory allocation:

```c
// Allocate 1MB of zero-initialized memory
void *mem = mmap(NULL, 1024 * 1024,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1, 0);
if (mem == MAP_FAILED) {
    perror("mmap");
    exit(1);
}
```

Anonymous mmap is page-aligned, and pages are lazily allocated (faulted in on first touch). This is more efficient than `malloc()` for large, long-lived allocations.

Key flags:
- `MAP_PRIVATE` — COW, changes not seen by others.
- `MAP_SHARED` — changes visible to other processes (shared memory IPC).
- `MAP_ANONYMOUS` — not backed by a file.
- `MAP_FIXED` — place mapping at exact address (dangerous; prefer `MAP_FIXED_NOREPLACE`).
- `MAP_POPULATE` — prefault pages into RAM immediately.
- `MAP_LOCKED` — lock pages in RAM (no swap). Requires `mlock()` rights.

```c
// Guard page — allocate 1 page with no access rights, catches buffer overruns
mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
```

## mprotect and Memory Protection

Change access permissions for a mapped region:

```c
int mprotect(void *addr, size_t len, int prot);
// prot = PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE
```

mprotect-based tricks:
- **Self-modifying code**: Write code, mprotect to `PROT_EXEC`.
- **Write barriers**: mprotect a page to `PROT_READ`, handle `SIGSEGV` to log access.
- **Stack guards**: The kernel maps a zero-permission page below the stack; touching it triggers expansion or a segfault.

## mlock: Keeping Memory Resident

Prevent the kernel from swapping out critical memory:

```c
mlock(ptr, size);        // lock into RAM
munlock(ptr, size);      // unlock
mlockall(MCL_CURRENT);   // lock all current mappings
munlockall();
```

Use sparingly. Requires `CAP_IPC_LOCK` capability or `RLIMIT_MEMLOCK` ulimit.

## NUMA Awareness

On **Non-Uniform Memory Access** (NUMA) systems, memory access latency varies by which CPU socket you're on and which memory bank holds the data. The kernel tries to allocate pages from "local" memory, but you can hint:

```c
#include <numaif.h>

// Pin thread to a specific NUMA node
mbind(addr, len, MPOL_BIND, nodemask, maxnode, 0);
```

NUMA is critical for multi-socket servers. Tools: `numactl`, `numastat`.

## Huge Pages

The default 4KB page size creates enormous page tables for large-memory applications. **Huge pages** (2MB or 1GB on x86-64) reduce TLB (Translation Lookaside Buffer) pressure:

```c
// Transparent Huge Pages (THP) — kernel manages automatically
echo always > /sys/kernel/mm/transparent_hugepage/enabled

// Explicit huge pages via mmap
void *addr = mmap(NULL, 2*1024*1024, PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
```

Huge pages reduce TLB misses by ~512x (2MB/4KB) but increase internal fragmentation for mixed-size workloads.

## Debugging Memory: Valgrind and AddressSanitizer

**Valgrind Memcheck** detects leaks, use-after-free, and uninitialized reads:

```bash
gcc -g prog.c -o prog
valgrind --leak-check=full ./prog
```

**AddressSanitizer (ASan)** instruments code at compile time for far lower overhead (~2x vs 20x):

```bash
gcc -fsanitize=address -g prog.c -o prog
./prog
# Shadow memory catches out-of-bounds, use-after-free, double-free, leaks
```

**LeakSanitizer** (part of ASan):
```bash
ASAN_OPTIONS=detect_leaks=1 ./prog
```

## Summary

Virtual memory is not just about isolation — it is a toolkit. `mmap()` gives you demand-paged file I/O, shared memory, and anonymous allocations. `mprotect()` locks down access. Huge pages accelerate large workloads. Understanding these primitives separates a competent C programmer from a systems programmer.

Next up: signals — the asynchronous interrupts of user space.
