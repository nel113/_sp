# 系統程式 — 期末總整 (Final Term Compilation)

**課程**：系統程式  
**學期**：114 學年下學期  
**學生**：盧隆勝（學號末兩碼：27）  
**教師**：[陳鍾誠](https://www.nqu.edu.tw/educsie/index.php?act=blog&code=list&ids=4)  
**學校**：[金門大學資訊工程系](https://www.nqu.edu.tw/educsie/index.php)  
**課程教材**：[cpu2os](https://github.com/ccc114b/cpu2os) · [AI Teach You SP](https://github.com/cccbook/ai-teach-you/blob/main/sp/tw/README.md) · [c0computer](https://github.com/ccc-c/c0computer)

---

## 目錄 / Table of Contents

| # | 作業 | 主題 | 語言 |
|---|------|------|------|
| 1 | [HW1: p0 Compiler](#hw1-p0-compiler) | 單遍編譯器：詞法分析 → 語法分析 → 堆疊機程式碼產生 | C |
| 2 | [HW2: Aura Language](#hw2-aura-language) | 自製直譯語言：Lexer + Compiler + Stack VM | Python |
| 3 | [HW3: Enhanced Aura](#hw3-enhanced-aura) | 擴展 Aura：函式、字串、邏輯運算、遞迴、模除 | Python |
| 4 | [HW4: System Programming Book](#hw4-the-art-of-system-programming) | 系統程式開放書籍：10 章含程式碼範例 | Markdown + C |
| 5 | [HW5: Concurrent Programming](#hw5-concurrent-programming) | 並行程式設計：Mutex、Condition Variable、死結預防 | C (pthread) |
| 6 | [HW6: Processes & File Descriptors](#hw6-processes--file-descriptors) | 行程管理與檔案描述子：fork/exec/pipe/dup2 | C |
| 7 | [Midterm: MiniShell](#midterm-minishell) | 命令列殼層：管線、重導向、內建指令、訊號處理 | C |

---

## 專案摘要 / Project Summary

本專案為系統程式課程的完整歷程，從底層編譯器實作到作業系統等級的行程管理，循序漸進地涵蓋了系統程式設計的核心概念。

### 學習軌跡

```
HW1 編譯器基礎 → HW2-3 語言設計與虛擬機 → HW4 系統程式理論
    → HW5 並行程式設計 → HW6 行程與檔案描述子 → Midterm 整合專案
```

### 核心概念覆蓋

| 概念 | HW1 | HW2 | HW3 | HW4 | HW5 | HW6 | Mid |
|------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 詞法/語法分析 | ✓ | ✓ | ✓ | | | | |
| 堆疊機程式碼產生 | ✓ | ✓ | ✓ | | | | |
| 虛擬機執行 | | ✓ | ✓ | | | | |
| 作業系統理論 | | | | ✓ | | | |
| 行程管理 (fork/exec/wait) | | | | ✓ | | ✓ | ✓ |
| 檔案描述子 (open/read/write/dup2) | | | | ✓ | | ✓ | ✓ |
| 管線 (pipe) | | | | ✓ | | ✓ | ✓ |
| 訊號處理 (signal) | | | | ✓ | | | ✓ |
| 執行緒與同步 (pthread/mutex/cond) | | | | ✓ | ✓ | | |
| 記憶體管理 (malloc/mmap) | | | | ✓ | | | |
| 網路程式設計 (socket/epoll) | | | | ✓ | | | |
| 系統呼叫深度解析 (strace/eBPF) | | | | ✓ | | | |
| 容器基礎 (namespace/cgroups) | | | | ✓ | | | |

---

## HW1: p0 Compiler

> **單遍編譯器**：將高階語言原始碼直接轉換為堆疊機組合語言，無中間 AST。

### 檔案結構
```
HW1/
├── compiler.c      # 編譯器原始碼 (215 行)
├── compiler         # 編譯後執行檔
└── README.md        # 架構文件
```

### 技術要點
- **Lexer** (`next()`): 解析識別符、數字、關鍵字 (`while`, `if`, `else`, `func`)、運算子
- **Parser**: 遞迴下降語法分析 (`program → func_decl → stmt → expr → factor`)
- **Code Generator**: 產生堆疊式虛擬機器碼 (PUSH, STORE, ADD, SUB, JZ, JMP, CALL, RET)
- **函式呼叫機制**: Prologue (PUSH BP / MOV BP SP) / Epilogue (MOV SP BP / POP BP / RET)
- **While 迴圈**: 標籤式跳轉 (JZ 條件出口 + JMP 迴圈返回)
- **變數定址**: 全域變數使用絕對位址，區域變數使用 BP 相對偏移

### 範例輸入
```c
func main() { i = 0; while (i < 10) { i = i + 1; } }
```

### 編譯與執行
```bash
gcc -Wall -Wextra -std=c99 -O2 compiler.c -o compiler
./compiler
```

---

## HW2: Aura Language

> **自製直譯語言**：使用 Python 實作完整的 Lexer → Compiler → Virtual Machine 管線。

### 檔案結構
```
HW2/
├── aura.py          # 語言實作 (339 行)
└── README.md         # 原始碼副本
```

### 技術要點
- **Lexer**: 支援 `let`, `show`, `if`, `else`, `loop` 關鍵字；整數數字；比較/算術運算子
- **Compiler**: 遞迴下降剖析，產生 bytecode tuples `(op, arg)`
  - 運算子優先權：`* /` > `+ -` > `<` > `==`
  - 支援 `if/else` (JZ + JMP 回補)、`loop` (JMP 迴圈 + JZ 出口)
- **Virtual Machine**: 堆疊式解譯器
  - 指令集：ICONST, LOAD, STORE, ADD, SUB, MUL, DIV, LT, EQ, JMP, JZ, PRINT, HALT
  - 使用 `globals` dict 儲存變數

### 範例 Aura 程式
```
let counter = 0;
let limit = 10;
loop (counter < limit) {
    counter = counter + 1;
    show counter;
}
```

### 執行
```bash
python3 HW2/aura.py
```

---

## HW3: Enhanced Aura

> **擴展 Aura 語言**：新增 7 大功能，使 Aura 成為更完整的程式語言。

### 檔案結構
```
HW3/
├── aura.py          # 增強版語言實作 (654 行)
└── README.md         # 功能說明文件
```

### 新增功能

| # | 功能 | 語法 | 新增指令 |
|---|------|------|----------|
| 1 | 字串字面值 | `"hello"`，支援跳脫序列 | SCONST |
| 2 | 函式定義與呼叫 | `func name(p) { return e; }` | CALL, RET, POP |
| 3 | 延伸比較 | `>`, `<=`, `>=`, `!=` | GT, LE, GE, NE |
| 4 | 邏輯運算 | `&&`, `||`, `!` | AND, OR, NOT, NEG |
| 5 | 模除運算 | `%` | MOD |
| 6 | 單行註解 | `// comment` | — |
| 7 | 使用者輸入 | `ask var;` | INPUT |

### 技術改進
- **8 級運算子優先權剖析器**：primary → unary → mul → add → comp → eq → and → or
- **函式呼叫堆疊**：VM 以 `call_stack` 儲存 `(return_ip, saved_globals)`
- **6 個展示程式**：模除與字串、邏輯比較、遞迴 Fibonacci (fib(10)=55)、多函式呼叫、迴圈含函式、HW2 回溯相容測試

### 執行
```bash
python3 HW3/aura.py
```

---

## HW4: The Art of System Programming

> **開放書籍**：從位元到行程，完整涵蓋 Linux 系統程式設計理論與實作。

### 檔案結構
```
HW4/
├── README.md                          # 書籍簡介
├── SUMMARY.md                         # 章節詳細大綱
├── LICENSE                            # CC BY-SA 4.0
├── chapters/
│   ├── 01-introduction.md             # 系統程式導論
│   ├── 02-processes.md                # 行程管理
│   ├── 03-filesystems-io.md           # 檔案系統與 I/O
│   ├── 04-memory-management.md        # 記憶體管理
│   ├── 05-signals-ipc.md              # 信號與行程間通訊
│   ├── 06-threads-concurrency.md      # 執行緒與並行
│   ├── 07-network-programming.md      # 網路程式設計
│   ├── 08-system-calls-deep-dive.md   # 系統呼叫深度解析
│   ├── 09-building-system-software.md # 建構系統軟體工具鏈
│   └── 10-advanced-topics.md          # 進階主題 (容器、eBPF)
└── code/
    ├── pipeline.c                     # fork+pipe+dup2 管線範例
    ├── epoll_echo_server.c            # epoll TCP echo 伺服器
    ├── raw_syscall_signal.c           # 裸系統呼叫與信號處理
    └── Makefile                       # 建構規則
```

### 各章節摘要

| 章節 | 核心內容 |
|------|----------|
| 01 導論 | 系統程式定義、OS 角色、為何用 C、系統呼叫介面、errno、編譯連結流程 |
| 02 行程 | fork/exec/wait、殭屍/孤兒行程、行程狀態轉換、守護行程化、/proc |
| 03 檔案系統 | "一切皆檔案"、檔案描述子、open/read/write/lseek、mmap、io_uring |
| 04 記憶體 | 虛擬記憶體佈局、brk/sbrk、malloc 內部、mmap、mprotect、Valgrind/ASan |
| 05 信號與 IPC | 信號處理 (sigaction)、管線、FIFO、System V IPC、POSIX IPC、Unix Socket |
| 06 執行緒 | pthread、競爭條件、Mutex/CondVar、原子操作 (C11)、死結預防、Futex |
| 07 網路 | Berkeley Socket、TCP echo 伺服器、select/poll/epoll、並行伺服器架構 |
| 08 系統呼叫 | x86-64 syscall 機制、clone()、vDSO、strace/ptrace、io_uring、eBPF |
| 09 工具鏈 | GCC/Clang、ELF 格式、靜態/動態函式庫、Make/CMake、GDB/perf/Valgrind |
| 10 進階 | Namespace、Cgroups v2、Capabilities、seccomp、容器安全堆疊、DPDK |

### 編譯與執行
```bash
cd HW4/code && make
./pipeline              # ls | sort
./epoll_echo_server     # TCP echo server on port 8080
./raw_syscall_signal    # raw syscall + signal demo
```

---

## HW5: Concurrent Programming

> **並行程式設計**：使用 POSIX Threads 實作三個經典同步問題。

### 檔案結構
```
HW5/
├── Makefile
├── README.md
├── docs/
│   ├── concepts.md         # 並行理論概念
│   └── implementation.md   # 實作設計文件
├── bank/
│   └── bank.c              # 銀行帳戶競爭條件展示
├── producer_consumer/
│   └── pc.c                # 有限緩衝區生產者-消費者
└── dining_philosophers/
    └── philosophers.c      # 哲學家就餐問題 (死結預防)
```

### 三程式概要

**1. Bank Account（銀行帳戶）**
- 兩個執行緒分別執行 100,000 次存款與提款
- 版本 A：使用 mutex 同步 → 最終餘額為 0
- 版本 B：無同步保護 → 展示競爭條件導致的不正確結果

**2. Producer-Consumer（生產者-消費者）**
- 2 個生產者 + 2 個消費者，緩衝區大小 5，共 40 個項目
- 同步機制：`pthread_mutex_t` + `pthread_cond_t` (not_full / not_empty)
- 使用環狀緩衝區，`while` 迴圈防禦虛假喚醒 (spurious wakeup)

**3. Dining Philosophers（哲學家就餐）**
- 5 位哲學家，5 支叉子，每人吃 3 次
- 死結預防：不對稱取叉策略（哲學家 4 先取右叉，其他人先取左叉）
- 每位哲學家循環：思考 → 取叉 → 進食 → 放叉

### 理論文件 (docs/)
- `concepts.md`：執行緒 vs 行程比較、競爭條件、Mutex 機制、死結四條件
- `implementation.md`：三程式設計細節、同步原語使用表、偽程式碼

### 編譯與執行
```bash
cd HW5 && make && make run
```

---

## HW6: Processes & File Descriptors

> **行程與檔案描述子程式設計**：fork/exec/wait、檔案 I/O、重導向、管線。

### 檔案結構
```
HW6/
├── Makefile
├── README.md
├── programs/
│   ├── process_basics.c   # fork + execvp + wait
│   ├── file_io.c          # open + read + write
│   ├── io_redirection.c   # dup2 重導向 stdin/stdout/stderr
│   └── pipeline.c         # pipe + fork + dup2 + execvp
└── bin/                    # 編譯後執行檔
```

### 四程式概要

**1. process_basics.c — 行程基礎**
- `fork()` 建立子行程，`execvp("ls", ...)` 替換行程映像
- 父行程以 `wait()` 等待子行程，檢查離開狀態 (WIFEXITED/WEXITSTATUS)

**2. file_io.c — 檔案 I/O**
- `open()` 建立/開啟檔案，`write()` 寫入多行內容
- 重新以 `O_RDONLY` 開啟，`read()` 迴圈讀取 256-byte 區塊，`write()` 輸出到 stdout

**3. io_redirection.c — I/O 重導向**
- 示範 1：stdout 重導向到檔案 (`dup2(fd, STDOUT_FILENO)`)
- 示範 2：stderr 重導向到檔案 (`dup2(fd, STDERR_FILENO)`)
- 使用 `dup()` 保存原始 fd 以便還原

**4. pipeline.c — 管線實作**
- 等同 Shell 指令 `ls | wc -l`
- `pipe()` 建立單向通道 → `fork()` 兩個子行程
- 子行程 1：stdout → pipe write end，execvp("ls")
- 子行程 2：stdin ← pipe read end，execvp("wc", ...)
- 父行程關閉管線兩端，`waitpid()` 等待子行程

### 檔案描述子快速參考
| fd | 常數 | 用途 |
|----|------|------|
| 0 | STDIN_FILENO | 標準輸入 |
| 1 | STDOUT_FILENO | 標準輸出 |
| 2 | STDERR_FILENO | 標準錯誤 |

### 編譯與執行
```bash
cd HW6 && make && make run
```

---

## Midterm: MiniShell

> **命令列殼層**：整合本學期所有系統程式概念的期末專案。

### 檔案結構
```
Midterm/
├── shell.c           # MiniShell 原始碼 (233 行)
├── minishell          # 編譯後執行檔
├── Makefile
└── README.md          # 完整專案文件 (373 行)
```

### 功能
- **內建指令**：`exit`、`cd`、`pwd`、`help`
- **外部程式執行**：`fork()` + `execvp()` + `waitpid()`
- **I/O 重導向**：`<` 輸入重導向、`>` 輸出重導向 (使用 `dup2`)
- **管線**：`|` 多指令管線 (使用 `pipe()` + `dup2()`)
- **訊號處理**：Ctrl+C 中斷處理 (async-signal-safe `write()`)

### 系統程式概念整合

| 概念 | 實作位置 | 使用系統呼叫 |
|------|----------|-------------|
| 行程建立 | `execute_external()` | fork, execvp |
| 行程等待 | `run_pipeline()` | waitpid |
| 檔案描述子重導向 | `execute_external()` | dup2, open |
| 管線 IPC | `run_pipeline()` | pipe |
| 訊號處理 | `setup_signals()` | sigaction |
| 記憶體管理 | `read_line()` | malloc, free |
| 錯誤處理 | 全部函式 | errno, perror |

### 支援語法範例
```bash
minishell> pwd
minishell> cd /tmp
minishell> ls -la > output.txt
minishell> cat < input.txt
minishell> ls | wc -l
minishell> ls -la | grep .c | wc -l > count.txt
minishell> exit
```

### 編譯與執行
```bash
cd Midterm && make && make run
```

### 測試
```bash
cd Midterm && make test   # 5 個自動化測試
```

---

## 快速開始 / Quick Start

```bash
# 編譯所有 C 程式
cd Finalterm/HW1 && gcc -Wall -Wextra -std=c99 -O2 compiler.c -o compiler
cd Finalterm/HW4/code && make
cd Finalterm/HW5 && make
cd Finalterm/HW6 && make
cd Finalterm/Midterm && make

# 執行 Python 程式
python3 Finalterm/HW2/aura.py
python3 Finalterm/HW3/aura.py
```

---

## 需求 / Requirements

- **C 程式**：GCC (支援 C99/C11/GNU99)、pthread 函式庫、Linux 環境
- **Python 程式**：Python 3.6+
- **書籍**：任何 Markdown 閱讀器
