# Chapter 9: Building System Software

## The Toolchain

Building C software involves four major tools:

```
source.c → cc (compiler) → object.o → ld (linker) → executable
                                    ↑
                        libc.a/libc.so (libraries)
```

- **Compiler** (gcc, clang): Translates C to object code.
- **Assembler** (as): Translates assembly to machine code (usually invoked by compiler).
- **Linker** (ld, lld, gold, mold): Combines object files, resolves symbols, produces executable.
- **Build System** (make, cmake, meson, bazel): Orchestrates the above.

## GCC and Clang

### GCC (GNU Compiler Collection)

The venerable standard. Produces robust, well-optimized code:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -g prog.c -o prog
```

### Clang

LLVM-based, with superior diagnostics and tooling support:

```bash
clang -std=c11 -Wall -Wextra -g -O2 prog.c -o prog
```

### Essential Flags

| Flag | Purpose |
|------|---------|
| `-Wall -Wextra` | Enable warnings |
| `-Werror` | Treat warnings as errors |
| `-O0` / `-Og` | No optimization / debug-friendly |
| `-O2` / `-O3` | Production optimization |
| `-Os` / `-Oz` | Size optimization |
| `-g` | Debug information (DWARF) |
| `-g3` | Max debug info (includes macros) |
| `-DNAME=VAL` | Define preprocessor macro |
| `-I/path` | Include search path |
| `-L/path` | Library search path |
| `-lname` | Link against libname |
| `-static` | Static linking |
| `-fPIC` | Position-independent code (for shared libs) |
| `-fsanitize=address` | AddressSanitizer |
| `-fsanitize=thread` | ThreadSanitizer |
| `-fsanitize=undefined` | Undefined Behavior Sanitizer |
| `-fstack-protector-strong` | Stack canaries |
| `-D_FORTIFY_SOURCE=2` | Buffer overflow detection |

## Compilation Stages, Inspected

`gcc -v prog.c -o prog` shows the detailed invocation:

```bash
# Preprocess
cc -E prog.c > prog.i

# Compile to assembly
cc -S prog.i -o prog.s

# Assemble
as prog.s -o prog.o

# Link
ld -dynamic-linker /lib64/ld-linux-x86-64.so.2 \
   /usr/lib/crt1.o /usr/lib/crti.o \
   prog.o -lc -o prog
```

Each `.o` file contains:
- Machine code in `.text`
- Initialized data in `.data`
- Read-only data in `.rodata`
- Uninitialized data in `.bss`
- Symbol table and relocation records

Inspect with:
```bash
objdump -d prog.o          # disassemble
objdump -t prog.o          # symbol table
objdump -r prog.o          # relocations
readelf -h prog            # ELF header
readelf -l prog            # program headers (segments)
readelf -S prog            # section headers
nm prog                    # symbol table
size prog                  # section sizes
```

## ELF: The Executable and Linkable Format

ELF is the standard binary format on Linux, BSD, and Solaris. An ELF file has up to three views:

1. **Linking view** — sections (for the linker): `.text`, `.data`, `.bss`, `.symtab`, `.rela.text`, `.strtab`.
2. **Execution view** — segments (for the kernel loader): `PT_LOAD` (load into memory), `PT_DYNAMIC` (dynamic linking info), `PT_INTERP` (dynamic linker path).
3. **Shared libraries** — PIC code with GOT/PLT for lazy symbol resolution.

```
ELF Header
Program Header Table      (execution view)
  Segment 1 (LOAD: .text + .rodata, R-X)
  Segment 2 (LOAD: .data + .bss, RW-)
  ...
Section Header Table       (linking view)
  .text
  .rodata
  .data
  .bss
  .symtab
  .strtab
  ...
```

## Static vs. Dynamic Libraries

### Static Libraries (.a)

An archive of `.o` files. At link time, the linker copies only the needed objects:

```bash
# Create
ar rcs libmylib.a foo.o bar.o

# Link
gcc prog.o -L. -lmylib -o prog
```

Static binaries are self-contained but:
- Larger on disk and in memory.
- Cannot share code pages between processes.
- Must be re-linked to pick up library updates.
- No startup overhead from dynamic loading.

### Shared Libraries (.so)

Position-independent code mapped into multiple processes:

```bash
# Create
gcc -shared -fPIC foo.c bar.c -o libmylib.so

# Install
cp libmylib.so /usr/local/lib/
ldconfig

# Link
gcc prog.c -L. -lmylib -o prog

# Runtime search
export LD_LIBRARY_PATH=/my/libs:$LD_LIBRARY_PATH
```

The dynamic linker (`ld.so`) maps the `.so` at a random base address (ASLR) and resolves symbols lazily through the PLT/GOT mechanism:

```
call printf@PLT  →  PLT stub  →  GOT[printf]
    (first call)       ↓            (unresolved)
                  _dl_runtime_resolve()  →  update GOT
    (subsequent calls)  →  GOT[printf] (direct jump)
```

Symbol visibility:
```c
__attribute__((visibility("default"))) void public_api(void);
__attribute__((visibility("hidden")))  void internal_fn(void);

// Or with -fvisibility=hidden and whitelist
```

## Make: The Classic Build System

```makefile
# Simple Makefile
CC     = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDLIBS = -lpthread

SRCS   = main.c utils.c network.c
OBJS   = $(SRCS:.c=.o)
TARGET = server

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
```

Key concepts:
- **Automatic variables**: `$@` (target), `$<` (first prerequisite), `$^` (all prerequisites).
- **Pattern rules**: `%.o: %.c` matches any `.o` file from its `.c`.
- **Phony targets**: `.PHONY` marks targets not corresponding to files.
- **Parallel builds**: `make -j$(nproc)`.
- **Dependency generation**:
  ```makefile
  %.d: %.c
      $(CC) -MM $< > $@
  include $(SRCS:.c=.d)
  ```

## CMake: Cross-Platform Build System

CMake generates build files (Makefiles, Ninja, Xcode projects, Visual Studio solutions):

```cmake
cmake_minimum_required(VERSION 3.16)
project(myserver C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

add_executable(server main.c utils.c network.c)
target_link_libraries(server PRIVATE pthread)

# Out-of-source build
# mkdir build && cd build && cmake .. && make
```

For libraries:
```cmake
add_library(mylib SHARED foo.c bar.c)
add_library(mylib_static STATIC foo.c bar.c)
target_include_directories(mylib PUBLIC include/)

# Install rules
install(TARGETS mylib LIBRARY DESTINATION lib)
install(FILES include/mylib.h DESTINATION include)
```

## Makefile Tips for System Programs

```makefile
# Use pkg-config for library detection (e.g., liburing)
CFLAGS  += $(shell pkg-config --cflags liburing)
LDLIBS  += $(shell pkg-config --libs liburing)

# Debug build vs release build
ifeq ($(BUILD),debug)
    CFLAGS += -Og -g3 -fsanitize=address
else
    CFLAGS += -O2 -DNDEBUG
endif

# Verbose build recipes
$(TARGET): $(OBJS)
    @echo "LINK $@"
    $(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
```

## Debugging with GDB

```bash
gcc -g prog.c -o prog
gdb ./prog
```

Essential GDB commands:

```
b main                   # breakpoint at main
b file.c:42              # breakpoint at line
b function               # breakpoint at function
r                        # run
c                        # continue
n                        # next (step over)
s                        # step (step into)
fin                      # finish current function
p variable               # print variable
p/x addr                 # print in hex
x/10x addr               # examine 10 hex words at addr
bt                       # backtrace
bt full                  # backtrace with locals
info threads             # list threads
thread 2                 # switch to thread 2
info registers           # dump registers
watch variable           # break when variable changes
set var x = 42           # modify variable
layout asm               # TUI: assembly + source
layout src               # TUI: source
```

## Profiling with perf

`perf` is Linux's performance counter tool:

```bash
# Sample on-CPU time
perf record -g ./prog       # record with call graph
perf report                  # interactive flame graph
perf stat ./prog             # counter summary

# Common metrics
perf stat -e cycles,instructions,cache-misses,branch-misses ./prog

# CPU-specific events
perf list                    # show available events

# Attach to running process
perf top -p <pid>
```

A high `cache-misses` rate indicates pointer-chasing data structures. A low `instructions-per-cycle` (IPC) suggests stalled pipelines. perf helps you find the bottleneck.

## Valgrind and Sanitizers

```bash
# Memory errors
valgrind --leak-check=full --track-origins=yes ./prog

# Cache and branch prediction simulation
valgrind --tool=cachegrind ./prog
cg_annotate cachegrind.out.<pid>

# Thread error detection
valgrind --tool=helgrind ./prog
```

Compiler sanitizers have lower overhead than Valgrind:

```bash
gcc -fsanitize=address   prog.c -o prog  # memory errors (2x slowdown)
gcc -fsanitize=thread    prog.c -o prog  # data races (5-15x slowdown)
gcc -fsanitize=undefined prog.c -o prog  # UB (minimal)
gcc -fsanitize=leak      prog.c -o prog  # memory leaks (minimal)
```

## Core Dumps

When a process crashes with `SIGSEGV`, `SIGABRT`, etc., it can produce a **core dump** — a snapshot of its memory at death:

```bash
ulimit -c unlimited              # enable core dumps
echo core > /proc/sys/kernel/core_pattern
./crashing_program               # produces "core"

gdb ./crashing_program core      # load the core
bt                               # backtrace at crash point
info registers                   # registers at crash
x/10i $rip                       # instructions around crash
```

Core dumps are forensic gold. They capture the exact state of all threads, the stack, heap, and open file descriptors. On systemd-based systems, use `coredumpctl` to manage.

## Summary

| Tool | Purpose |
|------|---------|
| gcc/clang | Compilation |
| make/cmake | Build orchestration |
| binutils | objdump, readelf, nm, size |
| gdb/lldb | Debugging |
| perf | CPU profiling |
| valgrind/asan | Memory and thread sanitizers |
| strace/ltrace | Syscall and library call tracing |
| objdump/readelf | Binary inspection |

Next: containers, eBPF, capabilities — the cutting edge of system programming.
