# Chapter 6: Threads and Concurrency

## Why Threads?

Processes are isolated, expensive to create, and communicate through IPC. **Threads** share the same address space — they see the same heap, globals, and file descriptors. This makes communication trivial (just write to a global variable) but introduces the need for synchronization.

Threads are lighter than processes because:
- Creating a thread is ~10x faster than `fork()` (no page table copy).
- Context switching between threads of the same process is cheaper (no TLB flush).
- Memory is shared by default — no IPC overhead.

## POSIX Threads (pthreads)

```c
#include <pthread.h>

void *worker(void *arg) {
    int id = *(int*)arg;
    printf("Thread %d running\n", id);
    return (void*)(intptr_t)(id * 2);
}

int main(void) {
    pthread_t tid;
    int arg = 42;

    pthread_create(&tid, NULL, worker, &arg);
    // Don't let arg go out of scope before the thread reads it!

    void *retval;
    pthread_join(tid, &retval);
    printf("Thread returned: %ld\n", (intptr_t)retval);
}
```

Compile with `-lpthread`:
```bash
gcc -lpthread threads.c -o threads
```

Threads exit by returning from the start routine, calling `pthread_exit()`, or being cancelled with `pthread_cancel()`. The last thread in a process exiting triggers `exit()` for the whole process (unless detached).

## The Race Condition

The classic problem: two threads incrementing a shared counter without synchronization.

```c
static int counter = 0;

void *increment(void *arg) {
    for (int i = 0; i < 1'000'000; i++)
        counter++;  // NOT atomic — read-modify-write
    return NULL;
}
// With two threads, counter is almost never 2,000,000
```

The assembly for `counter++` on x86-64:
```asm
mov    eax, [counter]    ; load
inc    eax               ; modify
mov    [counter], eax    ; store
```

Two threads interleaving can both load the same value, increment, and store, losing one update.

## Mutexes

A **mutex** (mutual exclusion lock) ensures only one thread executes a critical section at a time:

```c
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *safe_increment(void *arg) {
    for (int i = 0; i < 1'000'000; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}
```

Mutex flavor choices:
- `PTHREAD_MUTEX_NORMAL` — self-deadlock on relock, undefined on unlock-by-other.
- `PTHREAD_MUTEX_ERRORCHECK` — returns `EDEADLK` on double-lock (safer for debugging).
- `PTHREAD_MUTEX_RECURSIVE` — same thread can lock multiple times; must unlock same count.
- `PTHREAD_MUTEX_DEFAULT` — platform-defined (Linux: like NORMAL).

```c
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
pthread_mutex_init(&lock, &attr);
pthread_mutexattr_destroy(&attr);
```

## Condition Variables

When a thread must wait for a *condition* to become true (e.g., "queue is not empty"), busy-waiting wastes CPU. **Condition variables** provide an efficient wait/signal mechanism:

```c
pthread_mutex_t  mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t   cv  = PTHREAD_COND_INITIALIZER;
int              ready = 0;

// Consumer
void *consumer(void *arg) {
    pthread_mutex_lock(&mtx);
    while (!ready)                          // always loop, not if
        pthread_cond_wait(&cv, &mtx);       // atomically unlocks mtx and waits
    // consume...
    pthread_mutex_unlock(&mtx);
    return NULL;
}

// Producer
void *producer(void *arg) {
    pthread_mutex_lock(&mtx);
    ready = 1;
    pthread_mutex_unlock(&mtx);
    pthread_cond_signal(&cv);               // wake one waiter
    // pthread_cond_broadcast(&cv);        // wake all waiters
    return NULL;
}
```

The `while` loop (not `if`) is essential: it guards against **spurious wakeups** — the kernel may wake a waiting thread even without a signal. This is allowed by POSIX and happens on some platforms for efficiency.

## Read-Write Locks

When a data structure is read frequently and written rarely, a mutex causes unnecessary serialization of readers. An **rwlock** allows concurrent readers but exclusive writers:

```c
pthread_rwlock_t rwl = PTHREAD_RWLOCK_INITIALIZER;

// Reader
pthread_rwlock_rdlock(&rwl);
int val = shared_data;
pthread_rwlock_unlock(&rwl);

// Writer
pthread_rwlock_wrlock(&rwl);
shared_data = new_value;
pthread_rwlock_unlock(&rwl);
```

## Atomic Operations

For simple shared variables, atomic operations avoid mutex overhead entirely:

```c
#include <stdatomic.h>

atomic_int counter = 0;

void *increment(void *arg) {
    for (int i = 0; i < 1'000'000; i++)
        atomic_fetch_add(&counter, 1);  // lock-free, hardware guarantee
    return NULL;
}
```

Available operations: `atomic_load`, `atomic_store`, `atomic_fetch_add`, `atomic_compare_exchange_strong`, `atomic_compare_exchange_weak`. Use them with memory ordering constraints:

- `memory_order_relaxed` — no ordering guarantees, just atomicity (fastest).
- `memory_order_acquire` — all subsequent reads/writes stay after this load.
- `memory_order_release` — all prior reads/writes stay before this store.
- `memory_order_seq_cst` — total sequential consistency (safest, slowest).

```c
// Message-passing between threads
char data[256];
atomic_bool data_ready = false;

// Producer
sprintf(data, "payload");
atomic_store_explicit(&data_ready, true, memory_order_release);

// Consumer
while (!atomic_load_explicit(&data_ready, memory_order_acquire));
printf("%s\n", data);  // guaranteed to see the write to data[]
```

## Thread-Local Storage

```c
__thread int per_thread_counter = 0;  // GCC extension

// Or C11:
_Thread_local int per_thread_counter = 0;

// Or POSIX:
pthread_key_t key;
pthread_key_create(&key, destructor_function);
pthread_setspecific(key, value);
void *val = pthread_getspecific(key);
```

Each thread gets its own copy. Use for per-thread caches, random number generator states, and errno.

## Deadlock

A **deadlock** occurs when each thread in a set holds a resource and waits for another resource held by another thread in the set. Four necessary conditions (Coffman):

1. Mutual exclusion: resources cannot be shared.
2. Hold and wait: threads hold resources while waiting.
3. No preemption: resources cannot be forcibly taken.
4. Circular wait: T1→T2→...→Tn→T1 dependency chain.

Prevention strategies:
- **Lock ordering**: Always acquire locks in a consistent global order.
- **Try-lock and backoff**: `pthread_mutex_trylock()` returns `EBUSY` instead of blocking. If it fails, release held locks and retry.
- **Lock hierarchy**: Assign levels, never acquire a lock at a higher level while holding a lower one.
- **Timeouts**: `pthread_mutex_timedlock()` with a deadline.

```c
// Lock ordering: always lock A before B
void transfer(Account *from, Account *to, uint64_t amount) {
    Account *first  = (from->id < to->id) ? from : to;
    Account *second = (from->id < to->id) ? to : from;

    pthread_mutex_lock(&first->lock);
    pthread_mutex_lock(&second->lock);

    from->balance -= amount;
    to->balance   += amount;

    pthread_mutex_unlock(&second->lock);
    pthread_mutex_unlock(&first->lock);
}
```

## Thread Pools

Creating/destroying threads on every request is wasteful. Thread pools maintain a set of worker threads that pull tasks from a shared queue:

```c
typedef struct {
    void (*func)(void*);
    void *arg;
} Task;

typedef struct {
    Task           *tasks;
    int             capacity, size, head, tail;
    pthread_mutex_t mtx;
    pthread_cond_t  not_empty, not_full;
    bool            shutdown;
    pthread_t      *workers;
    int             num_workers;
} ThreadPool;
```

This pattern is the backbone of web servers, database connection pools, and async runtimes.

## Futex: The Kernel Primitive

pthread mutexes are not system calls. They use **futex** (fast userspace mutex) — an optimistic locking mechanism:

1. Atomic compare-and-swap in userspace grabs the lock if uncontended.
2. If contended, fall back to `futex(FUTEX_WAIT)` to block in the kernel.
3. Unlock uses `futex(FUTEX_WAKE)` to wake waiters.

This means uncontended lock/unlock takes ~25 CPU cycles — no kernel transition. `strace` on a threaded program won't show futex calls unless there's contention.

## Spinlocks

For extremely short critical sections (a few instructions), spinning beats blocking:

```c
pthread_spinlock_t spin;
pthread_spin_init(&spin, 0);
pthread_spin_lock(&spin);
// very short critical section
pthread_spin_unlock(&spin);
pthread_spin_destroy(&spin);
```

Spinlocks avoid the context-switch cost of futex but burn CPU while waiting. Only use inside the kernel or for microsecond-scale operations.

## Barriers

Synchronize a group of threads at a rendezvous point:

```c
pthread_barrier_t bar;
pthread_barrier_init(&bar, NULL, NUM_THREADS);

void *worker(void *arg) {
    do_phase_one();
    pthread_barrier_wait(&bar);  // all threads wait here
    do_phase_two();              // starts only after all reach the barrier
    return NULL;
}
```

## Summary

Threads are powerful but dangerous. The key to correct concurrent code:

1. **Share as little as possible** — message-passing architectures avoid shared state.
2. **Lock at the right granularity** — too coarse limits parallelism, too fine causes deadlocks.
3. **Use lock-free atomics when you can** — less overhead, no deadlock risk.
4. **Test with ThreadSanitizer**: `gcc -fsanitize=thread -g prog.c -o prog`

Next: network programming — communicating across machines.
