# Midterm Project: MiniShell — A System Programming Study

**Course:** System Programming (系統程式), 114 Spring Semester  
**Student:** 盧隆勝 (27)  
**Instructor:** 陳鍾誠, National Quemoy University CSIE

---

## 1. Introduction

MiniShell is a command-line interpreter (shell) written in C that demonstrates
the core system programming concepts covered in this course. It implements a
read-eval-print loop (REPL) capable of executing external programs, handling
I/O redirection, constructing pipelines, and managing processes — all through
Linux system calls at the POSIX layer.

### 1.1 Why a Shell?

A shell is the classic "capstone" of system programming education. To build a
working shell, you must understand and correctly orchestrate:

- **Process creation and lifecycle** (fork, exec, wait)
- **File descriptor abstraction and redirection** (dup2)
- **Inter-process communication via pipes** (pipe)
- **Signal handling** (sigaction)
- **Memory layout and buffering**

These map directly to the topics studied in HW4 (system programming
fundamentals), HW5 (concurrency), and HW6 (process + file descriptor
programming).

---

## 2. Architecture Overview

```
+------------------+
|   read_line()    |  read from stdin
+--------+---------+
         |
+--------v---------+
|  parse_pipe()    |  split on '|'
+--------+---------+
         |
+--------v---------+
|  parse_redirect()|  split on '<' and '>'
+--------+---------+
         |
+--------v---------+
|  run_builtin()   |  handle cd, exit, pwd, help
|  or              |
|  execute_external|  fork + execvp + dup2 + wait
+--------+---------+
         |
+--------v---------+
|  run_pipeline()  |  chain commands via pipe()
+--------+---------+
         |
         v
       REPL loop
```

### 2.1 Single Source File

The entire shell lives in one file (`shell.c`, ~190 SLOC) to keep the project
focused on concepts rather than build-system complexity. Each subsystem is a
small set of functions that can be studied in isolation.

---

## 3. Core System Programming Concepts

### 3.1 Process Creation: `fork()` + `execvp()` + `waitpid()`

Every external command runs in a **child process**. The parent (shell) never
replaces itself — it creates a clone with `fork()` and then calls `execvp()` in
the child.

```
Parent (shell)                    Child
    |                               |
    |  fork()  ------------------>  |  (copy of parent)
    |                               |
    |  waitpid(pid)                 |  dup2() for redirection
    |  (blocks until child exits)   |  execvp("ls", args)
    |                               |  (image replaced by /bin/ls)
    |                          [process ends]
    |
    |  returns to REPL loop
```

Key observation from `shell.c:115` (`execute_external`):

```c
pid_t pid = fork();
if (pid == 0) {
    // CHILD: redirect fds, then exec
    execvp(args[0], args);
    // execvp only returns on error
    exit(EXIT_FAILURE);
}
// PARENT: return pid so caller can wait
return pid;
```

### 3.2 File Descriptor Redirection: `dup2()`

The Unix "everything is a file" philosophy means stdin (fd 0), stdout (fd 1),
and stderr (fd 2) are just file descriptors. The shell replicates the `>` and
`<` operators by opening a target file and calling `dup2()` to replace the
standard descriptor.

```
Before:  fd 0 -> terminal     fd 1 -> terminal
After:   fd 0 -> input.txt     fd 1 -> output.txt
```

From `shell.c:120–130`:

```c
if (infile) {
    int fd = open(infile, O_RDONLY);
    dup2(fd, STDIN_FILENO);   // fd 0 now points to infile
    close(fd);
}
if (outfile) {
    int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(fd, STDOUT_FILENO);  // fd 1 now points to outfile
    close(fd);
}
```

An important subtlety: `close(fd)` is called after `dup2()` because the
descriptor has been duplicated onto the standard fd and the original handle is
no longer needed. Leaving it open is a resource leak.

### 3.3 Inter-Process Communication: `pipe()` + `dup2()`

Pipes are unidirectional byte streams that connect two file descriptors:
`pipefd[0]` (read end) and `pipefd[1]` (write end). The shell constructs a
pipeline by:

1. Creating a pipe for each adjacent pair of commands.
2. Redirecting the left command's stdout to the pipe's write end.
3. Redirecting the right command's stdin to the pipe's read end.
4. Closing all unused pipe ends in every process (critical for correctness).

```
ls | wc -l

  [ls] --stdout--> pipefd[1] ---- pipefd[0] --stdin--> [wc -l]
```

From `shell.c:145–175` (`run_pipeline`):

```c
for (int i = 0; i < n; i++) {
    if (i < n - 1) pipe(pipefd);           // create pipe for i -> i+1

    run_command(commands[i],
                prev_fd,                    // stdin from previous pipe
                (i < n - 1) ? pipefd[1] : -1); // stdout to next pipe

    if (prev_fd >= 0) close(prev_fd);      // close read end in parent
    if (i < n - 1) {
        close(pipefd[1]);                   // close write end in parent
        prev_fd = pipefd[0];                // read end becomes next input
    }
}
```

**Why closing unused ends matters:** If the parent keeps `pipefd[1]` open, the
downstream reader (e.g., `wc -l`) will never receive EOF and will hang forever.
This is one of the most common pipeline bugs.

### 3.4 Signal Handling: `sigaction()`

When the user presses Ctrl+C, the kernel delivers `SIGINT` to the foreground
process group. The shell must:

1. **Ignore SIGINT in the shell itself** (so Ctrl+C doesn't kill the shell).
2. **Allow the child to receive SIGINT** (so the running command can be
   interrupted).
3. **Reset signal disposition in the child** via `signal(SIGINT, SIG_DFL)`.

From `shell.c:18–25`:

```c
static void sigint_handler(int sig) {
    (void)sig;
    interrupted = 1;
    write(STDOUT_FILENO, "\n", 1);
}
```

We use `write()` instead of `printf()` because signal handlers execute in an
async-signal-safe context and `printf` is not reentrant.

### 3.5 Memory Layout: Stack vs Heap

The shell uses:

- **Stack:** Local variables (`char cwd[256]`, `pid_t pids[16]`) for fixed-size
  data with automatic lifetime.
- **Heap:** The line buffer (`malloc(MAX_LINE)`) because the input size is
  unknown at compile time. Each allocation is paired with `free()` in the REPL
  loop.

### 3.6 Exit Codes and Error Handling

Every system call is checked for failure:

| Call       | On failure       | Recovery            |
|------------|------------------|---------------------|
| `fork()`   | `perror` + abort | Cannot proceed       |
| `execvp()` | `perror` + exit  | Child exits with 1   |
| `pipe()`   | `perror` + skip  | Skip this pipeline   |
| `open()`   | `perror` + exit  | Child exits with 1   |
| `chdir()`  | `perror` only    | Continue REPL        |

This follows the "fail fast, fail visibly" principle taught throughout the
course.

---

## 4. Code Maps to Course Topics

| Concept                    | Course   | Location in shell.c       |
|----------------------------|----------|---------------------------|
| `fork()` + `execvp()`      | HW6 §1   | `execute_external():115`  |
| `wait()` / `waitpid()`     | HW6 §1   | `run_pipeline():155,172`  |
| `open()` / `close()`       | HW6 §2   | `execute_external():123`  |
| `dup2()` for redirection   | HW6 §3   | `execute_external():120`  |
| `pipe()` for IPC           | HW6 §4   | `run_pipeline():158`      |
| Process lifecycle          | HW4 §2   | Overall architecture      |
| Concurrency (parallelism)  | HW5      | Multi-process pipeline    |
| Signal handling            | HW4 §5   | `setup_signals():18`      |
| Buffering / I/O            | HW4 §3   | `fflush()` in REPL        |
| Build system (Makefile)    | HW4 §9   | `Makefile`                |

---

## 5. Building and Running

```bash
# Compile
make

# Run interactively
make run

# Run automated tests
make test

# Clean artifacts
make clean
```

**Requirements:**
- GCC or Clang with C99 support
- Linux / WSL / macOS (any POSIX system)

---

## 6. Demonstrations

### 6.1 Basic Command Execution

```
/home/user> ls -la
/home/user> echo "Hello, System Programming!"
/home/user> cat /proc/cpuinfo
```

### 6.2 I/O Redirection

```
/home/user> ls -la > /tmp/listing.txt
/home/user> cat < /tmp/listing.txt
/home/user> echo "one two three" > /tmp/words.txt
```

### 6.3 Pipelines

```
/home/user> ls | wc -l
/home/user> cat /etc/passwd | grep root | cut -d: -f7
/home/user> echo "a b c d e f" | tr ' ' '\n' | sort
```

### 6.4 Built-in Commands

```
/home/user> help
/home/user> pwd
/home/user> cd /tmp
/tmp> pwd
/tmp> cd ..
/home/user> exit
```

---

## 7. Design Decisions and Rationale

### 7.1 Why C instead of Python?

While the course uses both languages (HW1–3 in Python/C, HW4–6 in C), C was
chosen because:
1. System calls (`fork`, `exec`, `pipe`, `dup2`) are native POSIX C APIs.
2. Learning the raw interface builds deeper understanding than wrappers.
3. The course's HW6 already established the C codebase pattern.

### 7.2 Why Single-Pass Parsing?

The lexer uses `strtok()` which is destructive (modifies the input string).
This is acceptable because:
1. Each command line is a freshly allocated heap buffer.
2. We don't need to re-parse the same input.
3. It keeps the code compact — a full lexer/parser like HW1–3 would obscure
   the system programming focus.

A more robust shell would use a non-destructive tokenizer, but that belongs in
a compiler course rather than a system programming course.

### 7.3 Why No Job Control?

Job control (bg/fg, process groups, terminal foreground management via
`tcsetpgrp`) is a significant additional complexity layer. It is omitted here
to keep the project focused on the fundamental `fork`–`exec`–`pipe`–`dup2`
stack that every shell must implement first.

---

## 8. Learning Outcomes

Through this project, the student demonstrates:

1. **Process management** — Creating, waiting for, and replacing process images.
2. **File abstraction** — Treating files, pipes, and terminals uniformly via
   file descriptors.
3. **IPC fundamentals** — Building a data flow graph with anonymous pipes.
4. **Signal-safe programming** — Handling asynchronous signals without data
   races.
5. **Defensive coding** — Checking every system call return value, freeing
   resources, and closing unused file descriptors.
6. **Build engineering** — Writing a Makefile that supports build, run, test,
   and clean targets.

---

## 9. References

- Course material: [cpu2os](https://github.com/ccc114b/cpu2os)
- HW4: *The Art of System Programming*, Chapters 2–5, 8
- HW6: Process and File Descriptor Programming examples
- Stevens, W. R. & Rago, S. A. *Advanced Programming in the UNIX Environment*
- Kerrisk, M. *The Linux Programming Interface*

---

## 10. Future Extensions

Ideas for extending this shell (aligned with remaining course topics):

| Feature                | System Call / Concept           | Course Link |
|------------------------|---------------------------------|-------------|
| `&` background jobs    | `waitpid(WNOHANG)`, job table   | HW5 threads |
| `2>` stderr redirect   | `dup2(fd, STDERR_FILENO)`       | HW6 §3      |
| `>>` append mode       | `open(O_APPEND)`                | HW6 §2      |
| Environment variables  | `getenv()` / `setenv()`         | HW4 §2      |
| Script execution       | Parse from file, batch mode     | HW1–3       |
| Signal forwarding      | `kill()` to process group       | HW4 §5      |
