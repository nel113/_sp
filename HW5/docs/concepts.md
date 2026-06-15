# Threads, Race Conditions, Mutexes, and Deadlocks

## 1. Threads

A **thread** is the smallest unit of execution within a process. A process can
contain multiple threads that share the same address space (memory, file
descriptors, etc.) but execute independently.

### Key Properties

| Property       | Process                          | Thread                            |
|----------------|----------------------------------|-----------------------------------|
| Memory         | Separate address space           | Shared address space              |
| Creation cost  | High (fork + copy)               | Low (share most resources)        |
| Communication  | IPC (pipes, sockets, shared mem) | Direct shared-memory access       |
| Crash impact   | Isolated                         | Crashes the entire process        |

### Why Use Threads?

- **Parallelism**: Utilize multiple CPU cores for faster computation.
- **Responsiveness**: Keep a UI responsive while a background thread performs
  heavy work.
- **Resource sharing**: Threads naturally share data without complex IPC
  mechanisms.

### POSIX Threads (pthreads)

On Unix-like systems, the standard threading API is **pthreads**. Basic usage:

```c
#include <pthread.h>

void *worker(void *arg) {
    // Thread work here
    return NULL;
}

int main() {
    pthread_t tid;
    pthread_create(&tid, NULL, worker, NULL);
    pthread_join(tid, NULL);  // Wait for thread to finish
    return 0;
}
```

---

## 2. Race Condition

A **race condition** occurs when two or more threads access shared data
concurrently, and at least one access is a write. The final result depends on
the unpredictable order of thread execution (the "race").

### Example: Bank Account Without Synchronization

```c
int balance = 1000;

void *deposit(void *arg) {
    for (int i = 0; i < 50000; i++) {
        balance = balance + 1;  // NOT atomic!
    }
    return NULL;
}

void *withdraw(void *arg) {
    for (int i = 0; i < 50000; i++) {
        balance = balance - 1;  // NOT atomic!
    }
    return NULL;
}
```

Even though 50,000 deposits and 50,000 withdrawals should net to zero change,
the final balance is often **not** 1000. Why?

### The Three-Step Problem

`balance = balance + 1` compiles to three CPU instructions:

1. **LOAD** `balance` from memory into a register.
2. **ADD** 1 to the register.
3. **STORE** the register back to memory.

If two threads interleave:

```
Thread A: LOAD  balance → regA  (reads 1000)
Thread B: LOAD  balance → regB  (reads 1000 — stale!)
Thread A: ADD   1      → regA  (1001)
Thread B: ADD   1      → regB  (1001)
Thread A: STORE regA   → balance (1001)
Thread B: STORE regB   → balance (1001)
```

Both threads read the same value, so one increment is **lost**. After two
deposits, the balance is 1001 instead of 1002.

---

## 3. Mutex (Mutual Exclusion)

A **mutex** (mutual exclusion lock) is a synchronization primitive that
ensures only **one thread at a time** can access a critical section of code.

### How It Works

```
Thread A                 Thread B
─────────                ─────────
lock(mutex)              lock(mutex)  ← BLOCKED (waiting)
  critical section          :        ← waiting...
unlock(mutex)               :        ← now acquires lock
                          critical section
                         unlock(mutex)
```

### pthread Mutex API

```c
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *safe_worker(void *arg) {
    pthread_mutex_lock(&lock);
    // --- Critical section ---
    balance = balance + 1;
    // --- End critical section ---
    pthread_mutex_unlock(&lock);
    return NULL;
}
```

### Key Rules

1. **Always unlock** a mutex after locking, even on error paths.
2. **Minimize** the critical section — hold the lock for as little time as
   possible.
3. **Avoid nesting** locks unnecessarily (leads to deadlock).
4. **Do not unlock** a mutex you didn't lock.

### Mutex vs. Spinlock

| Mutex                              | Spinlock                        |
|------------------------------------|---------------------------------|
| Waiting thread **sleeps** (blocks) | Waiting thread **busy-waits**   |
| Good for longer critical sections  | Good for very short sections    |
| Higher overhead (context switch)   | Wastes CPU cycles while waiting |

---

## 4. Deadlock

A **deadlock** occurs when two or more threads are each waiting for a resource
held by another, creating a circular dependency where **none can proceed**.

### Four Necessary Conditions (Coffman Conditions)

All four must hold simultaneously for a deadlock to occur:

1. **Mutual Exclusion**: Resources cannot be shared.
2. **Hold and Wait**: A thread holds at least one resource while waiting for
   another.
3. **No Preemption**: Resources cannot be forcibly taken away.
4. **Circular Wait**: A cycle exists: Thread A waits for B, B waits for C,
   C waits for A.

### Classic Example

```c
pthread_mutex_t lockA, lockB;

// Thread 1                         // Thread 2
pthread_mutex_lock(&lockA);         pthread_mutex_lock(&lockB);
pthread_mutex_lock(&lockB);  ←      pthread_mutex_lock(&lockA);  ←
// DEADLOCK: T1 holds A,           // DEADLOCK: T2 holds B,
//           waits for B                      waits for A
```

### Prevention Strategies

| Strategy               | How                                                        |
|------------------------|------------------------------------------------------------|
| **Lock ordering**      | Always acquire locks in a fixed global order.              |
| **Lock timeout**       | Use `pthread_mutex_trylock()` or timed locks; back off.    |
| **Avoid nested locks** | Design so threads only hold one lock at a time.            |
| **Deadlock detection** | Build a wait-for graph; if a cycle appears, abort a thread.|

### The Dining Philosophers Problem

This classic problem (covered in our implementation section) is the canonical
illustration of deadlock. Five philosophers sit at a table with five forks.
Each philosopher needs **both** the left and right fork to eat. If every
philosopher picks up their left fork simultaneously, no one can pick up their
right fork — deadlock.

---

## Summary

| Concept        | Definition                                                   |
|----------------|--------------------------------------------------------------|
| **Thread**     | Lightweight unit of execution sharing process memory.        |
| **Race Condition** | Bug caused by unsynchronized concurrent access to shared data. |
| **Mutex**      | Lock ensuring only one thread enters a critical section.     |
| **Deadlock**   | Circular wait where threads are permanently blocked.         |

Understanding these four concepts is essential for writing correct and
efficient concurrent programs.
