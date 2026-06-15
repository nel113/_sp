# Implementation Documentation

## 1. Bank Account Simulation (`bank/bank.c`)

### Problem

Simulate bank deposits and withdrawals on the same account, running 100,000
deposit and 100,000 withdrawal operations concurrently using two threads.
The net effect should be zero — but without synchronization, race conditions
produce incorrect results.

### Implementation

- Two threads are created: one calls `deposit()` and the other calls
  `withdraw()`.
- Each function loops `NUM_TRANSACTIONS` (100,000) times, incrementing or
  decrementing the global `balance` variable.
- The program runs **two versions**:
  1. **With mutex**: `pthread_mutex_lock(&lock)` / `pthread_mutex_unlock(&lock)`
     wrap each `balance = balance ± 1` operation.
  2. **Without mutex**: The operations run unsynchronized.

### Key Synchronization

```c
pthread_mutex_lock(&lock);
balance = balance + 1;   // critical section
pthread_mutex_unlock(&lock);
```

The mutex ensures that only one thread at a time reads and writes `balance`,
making the compound operation atomic.

### Expected Output

```
[WITH mutex]    Final balance: 0  (expected: 0)
[WITHOUT mutex] Final balance: ???  (expected: 0, but wrong)
```

The unsynchronized version almost always produces a non-zero balance,
demonstrating the race condition visually.

---

## 2. Producer-Consumer Problem (`producer_consumer/pc.c`)

### Problem

Two **producer** threads generate items and place them into a bounded buffer.
Two **consumer** threads remove items from the buffer. Constraints:

1. Producers must **wait** when the buffer is full.
2. Consumers must **wait** when the buffer is empty.

### Implementation

#### Data Structures

```c
int buffer[BUFFER_SIZE];  // circular buffer
int count;                // current # of items
int in, out;              // write / read indices
```

#### Synchronization Primitives

| Primitive            | Purpose                                      |
|----------------------|----------------------------------------------|
| `pthread_mutex_t`    | Protects access to `buffer`, `count`, `in`, `out` |
| `pthread_cond_t not_full`  | Producers wait on this when buffer is full   |
| `pthread_cond_t not_empty` | Consumers wait on this when buffer is empty  |

#### Producer Logic

```
lock mutex
while (buffer is full):
    wait on not_full
insert item into buffer
signal not_empty
unlock mutex
```

#### Consumer Logic

```
lock mutex
while (buffer is empty):
    wait on not_empty
remove item from buffer
signal not_full
unlock mutex
```

#### Why `while` Instead of `if`?

We use `while (count == BUFFER_SIZE)` instead of `if (count == BUFFER_SIZE)`
to guard against **spurious wakeups** — a thread may be woken up even though
no signal was sent. The `while` loop re-checks the condition before proceeding.

### Execution Flow

1. Both producers and consumers start simultaneously.
2. Producers fill the buffer until it's full, then block on `not_full`.
3. Consumers remove items, signaling `not_full` so producers can resume.
4. The cycle continues until all items are produced and consumed.
5. Consumers detect the total count and exit.

---

## 3. Dining Philosophers Problem (`dining_philosophers/philosophers.c`)

### Problem

Five philosophers sit around a circular table. Between each pair of
philosophers is a fork (5 forks total). To eat, a philosopher needs **both**
the left and right fork. After eating, they put both forks down and think.

**The deadlock scenario**: If every philosopher picks up their left fork at
the same time, each will wait forever for the right fork — a circular
deadlock.

### Implementation

#### Shared Resources

```c
pthread_mutex_t forks[5];  // each fork is a mutex
```

#### Deadlock Prevention: Asymmetric Fork Acquisition

The standard solution breaks the **circular wait** condition by making one
philosopher acquire forks in reverse order:

```c
if (id == NUM_PHILOSOPHERS - 1) {
    // Last philosopher: right fork first, then left
    pthread_mutex_lock(&forks[right]);
    pthread_mutex_lock(&forks[left]);
} else {
    // Everyone else: left fork first, then right
    pthread_mutex_lock(&forks[left]);
    pthread_mutex_lock(&forks[right]);
}
```

Philosopher 4 (index `N-1`) picks up the **right** fork first. All others
pick up the **left** fork first. This asymmetry prevents a cycle in the
resource allocation graph.

#### Philosopher Lifecycle

```
loop MEALS times:
    THINK (random sleep)
    PICK UP forks (left/right, depending on id)
    EAT  (random sleep)
    PUT DOWN forks
END
```

### Alternative Solutions (Not Implemented)

| Strategy                       | Description                                   |
|--------------------------------|-----------------------------------------------|
| **Limit concurrency**          | At most N-1 philosophers may sit at the table. |
| **Try-lock + backoff**         | `pthread_mutex_trylock()` — if second fork fails, release the first. |

---

## Build & Run

All programs require `pthread`. Compile with:

```bash
make          # builds all three programs
make run      # runs all three
make clean    # removes binaries
```

Or manually:

```bash
gcc -pthread bank/bank.c -o bank/bank
gcc -pthread producer_consumer/pc.c -o producer_consumer/pc
gcc -pthread dining_philosophers/philosophers.c -o dining_philosophers/philosophers
```
