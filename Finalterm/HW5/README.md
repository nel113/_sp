# HW5 — Concurrent Programming with pthreads

This project demonstrates fundamental concurrency concepts using POSIX threads
(pthreads) in C.

## Contents

| Directory              | Description                                       |
|------------------------|---------------------------------------------------|
| `docs/concepts.md`     | Theory: threads, race conditions, mutex, deadlock |
| `docs/implementation.md` | How each program works (design + code walkthrough) |
| `bank/`                | Bank deposit/withdrawal simulation                |
| `producer_consumer/`   | Classic producer-consumer bounded-buffer problem   |
| `dining_philosophers/` | Dining philosophers with deadlock prevention       |

## Quick Start

```bash
make        # compile all programs
make run    # compile and run all programs
make clean  # remove binaries
```

## Requirements

- GCC (or any C compiler with pthread support)
- Linux / WSL / macOS
