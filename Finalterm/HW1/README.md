# Architectural and Implementation Documentation: The Enhanced p0 Compiler

This documentation provides an in-depth breakdown of the architecture, design patterns, and runtime mechanics of the single-pass p0 compiler. It details how the lexical analyzer, recursive descent parser, and stack-based code generator work in tandem to compile high-level programming constructs—specifically focusing on the newly implemented `while` loop syntax and the underlying function call mechanism.

---

## 1. High-Level Architecture Overview

The p0 compiler utilizes a **single-pass, recursive descent parsing architecture**. Unlike multi-pass compilers that construct an explicit Abstract Syntax Tree (AST) in memory before code generation, this compiler interleaves syntax verification and code emission.

```
[ Source Code String ] ──> [ Lexer (next) ] ──> [ Parser (stmt/expr) ] ──> [ Assembly Output ]

```

* **Lexer (Lexical Analyzer):** Tokenizes the raw stream of source characters into distinct semantic units (tokens).
* **Parser (Syntax Analyzer):** Enforces language grammar rules using a top-down approach.
* **Code Generator:** Emits virtual stack machine assembly instructions directly during the parsing process.

---

## 2. Lexical Analysis (The Lexer)

The lexical analyzer is driven by the global state pointers `src` (the base address of the source string) and `p` (the current evaluation character pointer).

### Token Representation

Tokens are classified using an enumeration mapping string constructs to unique integer identifiers:

```c
enum {
    Id, Num, While, If, Else, Func, 
    Assign, Add, Sub, Mul, Div, Eq, Neq, Lt, Gt, 
    Lpar, Rpar, Lbrace, Rbrace, Semi, Eof
};

```

### The `next()` Engine

Calling `next()` advances the pointer `p` through the source code string. It performs three primary functions:

1. **Whitespace Skipping:** Filters out spaces, newlines (`\n`), tabs (`\t`), and carriage returns (`\r`).
2. **Identifier & Keyword Resolution:** When encountering an alphabetic character, it reads consecutive alphanumeric characters into `token_str`. It then performs string comparisons to differentiate reserved keywords (`while`, `if`, `else`, `func`) from user-defined variables (`Id`).
3. **Operator and Symbol Matching:** Recognizes single-character operators (`+`, `-`, `{`, `}`) and multi-character conditional operators (`==`, `!=`).

---

## 3. Syntax Analysis & Code Generation

The grammar of the p0 compiler is defined using Extended Backus-Naur Form (EBNF). The recursive descent parser implements one C function per major grammar rule.

### 3.1 Expression Compilation (`expr()` and `factor()`)

The compiler assumes a stack-based target architecture. When evaluating an expression like `i = i + 1`:

* `factor()` identifies `i` as an identifier or `1` as a literal number and emits a `PUSH` instruction.
* `expr()` handles binary infix operators. After two operands are pushed onto the execution stack, it emits the corresponding operation (e.g., `ADD`), which implicitly pops both values, computes the result, and pushes it back onto the stack.

---

### 3.2 The `while` Loop Implementation

The addition of the `while` loop introduces structured control flow and conditional branching.

#### EBNF Grammar

```ebnf
WhileStmt = "while" "(" Expr ")" Stmt ;

```

#### Compilation Design and Control Flow

To transform a high-level looping structure into linear assembly instructions, the compiler relies on unique jump labels (`L0`, `L1`, `L2`, etc.) generated sequentially by `next_label()`.

The `while_stmt()` function manages compilation using the following internal execution pipeline:

```c
void while_stmt() {
    int l_start = next_label(); // Label marking condition re-evaluation
    int l_end = next_label();   // Label marking loop termination exit

    emit_label(l_start);        // 1. Emit target label for iteration loopback
    match(While);
    match(Lpar);
    expr();                     // 2. Compile condition (leaves result on stack)
    match(Rpar);
    
    char jz_inst[20];
    sprintf(jz_inst, "JZ L%d", l_end);
    emit(jz_inst);            // 3. Emit Jump-if-Zero conditional branch

    stmt();                     // 4. Recursively compile the inner loop body

    char jmp_inst[20];
    sprintf(jmp_inst, "JMP L%d", l_start);
    emit(jmp_inst);           // 5. Emit absolute jump back to condition check
    
    emit_label(l_end);          // 6. Emit target exit label
}

```

#### Code Generation Breakdown:

1. **`emit_label(l_start)`**: Establishes a checkpoint in assembly. Every loop iteration must return to this exact address to check whether execution should continue.
2. **`expr()`**: Evaluates the logical condition within the loop header. If `i < 10` is true, a non-zero value (typically `1`) rests on top of the stack. If false, `0` is left.
3. **`JZ L_end`**: The `JZ` (Jump if Zero) instruction evaluates the top stack value. If the value is `0` (false), the execution context immediately branches to the memory address designated by `L_end`.
4. **`stmt()`**: Processes the statements contained inside the loop's block.
5. **`JMP L_start`**: Forces an unconditional branch back to the loop's structural entry point to re-evaluate the conditional state.

---

## 4. Function Call Mechanism & Runtime Memory Management

Understanding how functions work in the p0 architecture requires analyzing how data is organized in virtual memory during runtime execution.

### 4.1 The Stack Frame (Activation Record)

Whenever a function is invoked, a designated region of memory is carved out on the program's runtime Call Stack. This layout is formally termed an **Activation Record** or a **Stack Frame**.

The state of a running function is governed by two registers:

* **Stack Pointer (SP):** Points dynamically to the top item currently residing on the stack. It fluctuates rapidly as expressions push and pop data.
* **Base Pointer / Frame Pointer (BP):** Acts as a fixed hardware anchor pointing to the base address of the current function's execution frame. It remains static throughout the life of the function call.

---

### 4.2 The Function Lifecycle (Prologue vs. Epilogue)

The implementation inside `func_decl()` demonstrates exactly how code generation preserves execution states during a function boundary cross.

```c
void func_decl() {
    match(Func);
    printf("\n%s:\n", token_str);
    match(Id); match(Lpar); match(Rpar);
    
    // --- FUNCTION PROLOGUE ---
    emit("PUSH BP");          // Save the caller's stack frame anchor
    emit("MOV BP, SP");       // Establish the new local frame anchor
    
    block();                  // Compile inner code context
    
    // --- FUNCTION EPILOGUE ---
    emit("MOV SP, BP");       // Deallocate local storage structures
    emit("POP BP");           // Restore the caller's stack frame anchor
    emit("RET");              // Branch back to saved return address
}

```

#### Step-by-Step Runtime Execution:

1. **Prologue Phase:**
* **`PUSH BP`**: The caller function's frame anchor address is pushed onto the stack to ensure that when this newly called function terminates, the old execution frame can be completely recovered.
* **`MOV BP, SP`**: The CPU shifts the Base Pointer forward to align with the current Stack Pointer. This creates a boundary line between the old function's space and the new function's space.


2. **Execution Phase (`block()`):**
* Local variable spaces are allocated on the stack below the Base Pointer. The compiled instructions execute local algorithms using this freshly allocated frame.


3. **Epilogue Phase:**
* **`MOV SP, BP`**: The compiler safely collapses the stack by pulling the Stack Pointer back to the Base Pointer anchor. This implicitly destroys all local variables allocated during execution.
* **`POP BP`**: The old Base Pointer address is popped from the stack and loaded back into the CPU's `BP` register. The caller function's structural frame layout is now fully restored.
* **`RET`**: The program fetches the hardware return address (pushed implicitly by a `CALL` instruction) off the stack and updates the Instruction Pointer register to resume operations in the parent calling module.



---

### 4.3 Scope Resolution & Local Variable Lookup

In production systems, compilers use the base pointer anchor to address variables in memory.

* **Global Variables:** Referenced via absolute fixed symbols or memory addresses determined at link time.
* **Local Variables:** Referenced via **relative offsets** originating from the current Base Pointer register (e.g., `[BP - 4]`, `[BP - 8]`).

Because the `SP` register shifts constantly whenever an arithmetic operation executes (e.g., intermediate stack values during evaluation), using it as a reference baseline for variable tracking is impossible. By caching the current execution offset relative to the static `BP` pointer within a symbol table entry, local variables can always be accurately targeted regardless of fluctuating data depths inside the evaluation stack.