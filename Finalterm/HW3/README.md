# HW3 — Enhanced Aura Language

Continuation of the HW2 Aura programming language project. Adds 7 major features to the original language while maintaining full backward compatibility.

---

## New Features Added in HW3

### 1. String Literals
- Double-quoted strings: `"Hello, World!"`
- Escape sequences: `\n`, `\t`, `\"`, `\\`
- String concatenation via the `+` operator
- New bytecode instruction: `SCONST` (string constant push)

### 2. Functions (`func`, `return`, `call`)
- Define functions with `func name(params) { ... }`
- Call functions with `name(args)` as expressions or statements
- Return values with `return expr;` (bare `return;` returns 0)
- Recursive function calls (e.g. Fibonacci, factorial)
- Multiple parameters supported via comma separation
- New bytecode instructions: `CALL`, `RET`, `POP`

### 3. Extended Comparison Operators
- `>`  — greater than
- `<=` — less than or equal
- `>=` — greater than or equal
- `!=` — not equal
- New bytecode instructions: `GT`, `LE`, `GE`, `NE`

### 4. Logical Operators
- `&&` — logical AND
- `||` — logical OR
- `!`  — logical NOT (unary)
- New bytecode instructions: `AND`, `OR`, `NOT`

### 5. Modulo Operator
- `%` — remainder (modulo)
- New bytecode instruction: `MOD`

### 6. Comments
- Single-line comments with `//`
- Everything from `//` to end of line is ignored

### 7. User Input (`ask`)
- `ask variable_name;` reads a line from stdin
- Automatically converts to integer if possible, otherwise stores as string
- New bytecode instruction: `INPUT`

---

## Architecture (unchanged 3-phase pipeline)

```
Source Code → Lexer → Compiler → Virtual Machine
```

### What changed in each phase:

| Phase | HW2 | HW3 Additions |
|-------|-----|---------------|
| **Lexer** | 10 token types | +15 token types (STR, FUNC, RETURN, ASK, COMMA, GT, LE, GE, NE, AND, OR, NOT, MOD, NE), string lexing, comment skipping, line/col tracking |
| **Compiler** | Recursive-descent for 2 precedence levels | Full precedence-climbing parser (8 levels: primary → unary → mul → add → comp → eq → and → or), function definition/call compilation, function table |
| **VM** | Stack machine, 12 instructions | +15 instructions (SCONST, MOD, GT, LE, GE, NE, AND, OR, NOT, NEG, CALL, RET, POP, INPUT), call stack for function context save/restore |

### Operator Precedence (lowest → highest)
1. `||` — logical OR
2. `&&` — logical AND
3. `==`, `!=` — equality
4. `<`, `>`, `<=`, `>=` — comparison
5. `+`, `-` — addition
6. `*`, `/`, `%` — multiplication
7. `!`, `-` (unary) — negation
8. Primary — literals, identifiers, `(...)`, function calls

---

## Demo Programs (all run automatically)

| # | Feature | Output |
|---|---------|--------|
| 1 | Modulo & strings | `1`, `Hello, Aura!` |
| 2 | Comparisons & logic | 9 boolean values |
| 3 | Recursive Fibonacci | `fib(10) = 55` |
| 4 | Multi-function & string concat | `Hello, World!`, `49`, `25` |
| 5 | Loop with function call | Even numbers 2–10 |
| 6 | Original HW2 code | `1, 2, 3, 999` (backward compatible) |

---

## Running

```bash
python aura.py
```

Runs all 6 demos, printing source code, bytecode, function table, and execution output for each.

---

## Example Aura Code (HW3 style)

```
// Define a recursive factorial function
func fact(n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

func greet(name) {
    return "Hi, " + name + "!";
}

show greet("Aura");
show fact(5);

let x = 17 % 5;
show x;

// Read user input
ask age;
show "You entered:";
show age;
```
