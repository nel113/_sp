# HW6 — Process and File Descriptor Programming

System-level programming exercises covering process creation (fork/execvp), file I/O
(open/close/read/write), and I/O redirection (dup2).

## Contents

| File                      | Description                                                      |
|---------------------------|------------------------------------------------------------------|
| `programs/process_basics.c` | fork(), execvp(), wait() – process creation & image replacement |
| `programs/file_io.c`       | open(), close(), read(), write() – low-level file I/O           |
| `programs/io_redirection.c` | dup2() – redirect stdin (0), stdout (1), stderr (2)            |
| `programs/pipeline.c`      | pipe() + fork() + dup2() + execvp() – build a shell pipeline    |

## File Descriptors Quick Reference

| FD  | Name   | Constant  | Purpose              |
|-----|--------|-----------|----------------------|
| 0   | stdin  | STDIN_FILENO  | Standard input       |
| 1   | stdout | STDOUT_FILENO | Standard output      |
| 2   | stderr | STDERR_FILENO | Standard error       |

## Quick Start

```bash
make          # compile all programs
make run      # compile and run all demos
make clean    # remove binaries
```

## Requirements

- GCC (with POSIX support)
- Linux / WSL / macOS
