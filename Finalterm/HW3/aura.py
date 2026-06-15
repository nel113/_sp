import sys

# =====================================================================
# 1. Lexer (Tokenizer) — extended with strings, new ops, functions
# =====================================================================
class Token:
    def __init__(self, type_, value):
        self.type = type_
        self.value = value

    def __repr__(self):
        return f"Token({self.type}, {self.value})"

LET, SHOW, IF, ELSE, LOOP = "LET", "SHOW", "IF", "ELSE", "LOOP"
FUNC, RETURN, ASK = "FUNC", "RETURN", "ASK"
IDENT, NUM, STR = "IDENT", "NUM", "STR"
ASSIGN, PLUS, MINUS, MUL, DIV, MOD = "ASSIGN", "PLUS", "MINUS", "MUL", "DIV", "MOD"
EQ, NE, LT, GT, LE, GE = "EQ", "NE", "LT", "GT", "LE", "GE"
AND, OR, NOT = "AND", "OR", "NOT"
LPAREN, RPAREN, LBRACE, RBRACE, SEMI, COMMA = "LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMI", "COMMA"
EOF = "EOF"

KEYWORDS = {
    "let": LET, "show": SHOW, "if": IF, "else": ELSE, "loop": LOOP,
    "func": FUNC, "return": RETURN, "ask": ASK
}

class Lexer:
    def __init__(self, text):
        self.text = text
        self.pos = 0
        self.line = 1
        self.col = 1
        self.current_char = self.text[0] if text else None

    def advance(self):
        if self.current_char == '\n':
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        self.pos += 1
        self.current_char = self.text[self.pos] if self.pos < len(self.text) else None

    def skip_whitespace(self):
        while self.current_char and self.current_char.isspace():
            self.advance()

    def skip_comment(self):
        while self.current_char and self.current_char != '\n':
            self.advance()

    def identifier(self):
        result = ""
        while self.current_char and (self.current_char.isalnum() or self.current_char == '_'):
            result += self.current_char
            self.advance()
        token_type = KEYWORDS.get(result, IDENT)
        return Token(token_type, result)

    def number(self):
        result = ""
        while self.current_char and self.current_char.isdigit():
            result += self.current_char
            self.advance()
        return Token(NUM, int(result))

    def string(self):
        self.advance()
        result = ""
        while self.current_char and self.current_char != '"':
            if self.current_char == '\\' and self.pos + 1 < len(self.text):
                self.advance()
                esc = {'n': '\n', 't': '\t', '"': '"', '\\': '\\'}
                result += esc.get(self.current_char, self.current_char)
            else:
                result += self.current_char
            self.advance()
        if self.current_char != '"':
            raise Exception(f"Lexical Error at {self.line}:{self.col}: Unterminated string")
        self.advance()
        return Token(STR, result)

    def get_next_token(self):
        while self.current_char:
            if self.current_char.isspace():
                self.skip_whitespace()
                continue
            if self.current_char == '/' and self.pos + 1 < len(self.text) and self.text[self.pos + 1] == '/':
                self.skip_comment()
                continue
            if self.current_char.isalpha():
                return self.identifier()
            if self.current_char.isdigit():
                return self.number()
            if self.current_char == '"':
                return self.string()

            if self.current_char == '=':
                self.advance()
                if self.current_char == '=':
                    self.advance()
                    return Token(EQ, "==")
                return Token(ASSIGN, "=")

            if self.current_char == '!':
                self.advance()
                if self.current_char == '=':
                    self.advance()
                    return Token(NE, "!=")
                return Token(NOT, "!")

            if self.current_char == '<':
                self.advance()
                if self.current_char == '=':
                    self.advance()
                    return Token(LE, "<=")
                return Token(LT, "<")

            if self.current_char == '>':
                self.advance()
                if self.current_char == '=':
                    self.advance()
                    return Token(GE, ">=")
                return Token(GT, ">")

            if self.current_char == '&':
                self.advance()
                if self.current_char == '&':
                    self.advance()
                    return Token(AND, "&&")
                raise Exception(f"Lexical Error at {self.line}:{self.col}: Expected '&&'")

            if self.current_char == '|':
                self.advance()
                if self.current_char == '|':
                    self.advance()
                    return Token(OR, "||")
                raise Exception(f"Lexical Error at {self.line}:{self.col}: Expected '||'")

            single_chars = {
                '+': PLUS, '-': MINUS, '*': MUL, '%': MOD,
                '(': LPAREN, ')': RPAREN, '{': LBRACE, '}': RBRACE,
                ';': SEMI, ',': COMMA
            }
            if self.current_char in single_chars:
                tok = Token(single_chars[self.current_char], self.current_char)
                self.advance()
                return tok

            if self.current_char == '/':
                self.advance()
                return Token(DIV, "/")

            raise Exception(f"Lexical Error at {self.line}:{self.col}: Unknown character '{self.current_char}'")

        return Token(EOF, None)


# =====================================================================
# 2. Compiler (Parser -> Bytecode Generator) — extended
# =====================================================================
class Compiler:
    def __init__(self, lexer):
        self.lexer = lexer
        self.current_token = self.lexer.get_next_token()
        self.bytecode = []
        self.function_table = {}

    def error(self, message):
        raise Exception(f"Syntax/Compile Error: {message}")

    def consume(self, token_type):
        if self.current_token.type == token_type:
            self.current_token = self.lexer.get_next_token()
        else:
            self.error(f"Expected token {token_type}, got {self.current_token.type} ('{self.current_token.value}')")

    def emit(self, instruction):
        self.bytecode.append(instruction)

    def current_address(self):
        return len(self.bytecode)

    def patch(self, address, target_value):
        op, _ = self.bytecode[address]
        self.bytecode[address] = (op, target_value)

    # --- Expressions (precedence climbing) ---
    def primary(self):
        token = self.current_token
        if token.type == NUM:
            self.consume(NUM)
            self.emit(("ICONST", token.value))
        elif token.type == STR:
            self.consume(STR)
            self.emit(("SCONST", token.value))
        elif token.type == IDENT:
            name = token.value
            self.consume(IDENT)
            if self.current_token.type == LPAREN:
                self.consume(LPAREN)
                arg_count = 0
                if self.current_token.type != RPAREN:
                    self.expr()
                    arg_count += 1
                    while self.current_token.type == COMMA:
                        self.consume(COMMA)
                        self.expr()
                        arg_count += 1
                self.consume(RPAREN)
                self.emit(("CALL", (name, arg_count)))
            else:
                self.emit(("LOAD", name))
        elif token.type == LPAREN:
            self.consume(LPAREN)
            self.expr()
            self.consume(RPAREN)
        elif token.type in (MINUS, NOT):
            op_type = token.type
            self.consume(op_type)
            self.primary()
            self.emit(("NEG" if op_type == MINUS else "NOT", None))
        else:
            self.error(f"Unexpected token '{token.value}'")

    def mul_expr(self):
        self.primary()
        while self.current_token.type in (MUL, DIV, MOD):
            op_type = self.current_token.type
            self.consume(op_type)
            self.primary()
            op_map = {MUL: "MUL", DIV: "DIV", MOD: "MOD"}
            self.emit((op_map[op_type], None))

    def add_expr(self):
        self.mul_expr()
        while self.current_token.type in (PLUS, MINUS):
            op_type = self.current_token.type
            self.consume(op_type)
            self.mul_expr()
            self.emit(("ADD" if op_type == PLUS else "SUB", None))

    def comp_expr(self):
        self.add_expr()
        comp_ops = {LT: "LT", GT: "GT", LE: "LE", GE: "GE"}
        while self.current_token.type in comp_ops:
            op_type = self.current_token.type
            self.consume(op_type)
            self.add_expr()
            self.emit((comp_ops[op_type], None))

    def eq_expr(self):
        self.comp_expr()
        while self.current_token.type in (EQ, NE):
            op_type = self.current_token.type
            self.consume(op_type)
            self.comp_expr()
            self.emit(("EQ" if op_type == EQ else "NE", None))

    def and_expr(self):
        self.eq_expr()
        while self.current_token.type == AND:
            self.consume(AND)
            self.eq_expr()
            self.emit(("AND", None))

    def expr(self):
        self.and_expr()
        while self.current_token.type == OR:
            self.consume(OR)
            self.and_expr()
            self.emit(("OR", None))

    # --- Statements ---
    def statement(self):
        if self.current_token.type == LET:
            self.consume(LET)
            var_name = self.current_token.value
            self.consume(IDENT)
            self.consume(ASSIGN)
            self.expr()
            self.consume(SEMI)
            self.emit(("STORE", var_name))

        elif self.current_token.type == IDENT:
            var_name = self.current_token.value
            self.consume(IDENT)
            if self.current_token.type == ASSIGN:
                self.consume(ASSIGN)
                self.expr()
                self.consume(SEMI)
                self.emit(("STORE", var_name))
            elif self.current_token.type == LPAREN:
                self.consume(LPAREN)
                arg_count = 0
                if self.current_token.type != RPAREN:
                    self.expr()
                    arg_count += 1
                    while self.current_token.type == COMMA:
                        self.consume(COMMA)
                        self.expr()
                        arg_count += 1
                self.consume(RPAREN)
                self.consume(SEMI)
                self.emit(("CALL", (var_name, arg_count)))
                self.emit(("POP", None))
            else:
                self.error(f"Expected '=' or '(' after identifier '{var_name}'")

        elif self.current_token.type == SHOW:
            self.consume(SHOW)
            self.expr()
            self.consume(SEMI)
            self.emit(("PRINT", None))

        elif self.current_token.type == ASK:
            self.consume(ASK)
            var_name = self.current_token.value
            self.consume(IDENT)
            self.consume(SEMI)
            self.emit(("INPUT", var_name))

        elif self.current_token.type == IF:
            self.consume(IF)
            self.consume(LPAREN)
            self.expr()
            self.consume(RPAREN)
            jz_addr = self.current_address()
            self.emit(("JZ", 0))
            self.block()
            if self.current_token.type == ELSE:
                self.consume(ELSE)
                jmp_addr = self.current_address()
                self.emit(("JMP", 0))
                self.patch(jz_addr, self.current_address())
                self.block()
                self.patch(jmp_addr, self.current_address())
            else:
                self.patch(jz_addr, self.current_address())

        elif self.current_token.type == LOOP:
            self.consume(LOOP)
            start_addr = self.current_address()
            self.consume(LPAREN)
            self.expr()
            self.consume(RPAREN)
            jz_addr = self.current_address()
            self.emit(("JZ", 0))
            self.block()
            self.emit(("JMP", start_addr))
            self.patch(jz_addr, self.current_address())

        elif self.current_token.type == RETURN:
            self.consume(RETURN)
            if self.current_token.type != SEMI:
                self.expr()
            else:
                self.emit(("ICONST", 0))
            self.consume(SEMI)
            self.emit(("RET", None))

        elif self.current_token.type == FUNC:
            self.consume(FUNC)
            func_name = self.current_token.value
            self.consume(IDENT)
            self.consume(LPAREN)
            params = []
            if self.current_token.type != RPAREN:
                params.append(self.current_token.value)
                self.consume(IDENT)
                while self.current_token.type == COMMA:
                    self.consume(COMMA)
                    params.append(self.current_token.value)
                    self.consume(IDENT)
            self.consume(RPAREN)
            jmp_over = self.current_address()
            self.emit(("JMP", 0))
            func_start = self.current_address()
            self.function_table[func_name] = (func_start, len(params), params)
            self.block()
            self.emit(("RET", None))
            self.patch(jmp_over, self.current_address())

        elif self.current_token.type == LBRACE:
            self.block()
        else:
            self.error(f"Invalid statement starter '{self.current_token.value}'")

    def block(self):
        self.consume(LBRACE)
        while self.current_token.type != RBRACE and self.current_token.type != EOF:
            self.statement()
        self.consume(RBRACE)

    def compile(self):
        while self.current_token.type != EOF:
            self.statement()
        self.emit(("HALT", None))
        return self.bytecode, self.function_table


# =====================================================================
# 3. Virtual Machine — extended with call stack, strings, functions
# =====================================================================
class VirtualMachine:
    def __init__(self, bytecode, function_table):
        self.bytecode = bytecode
        self.stack = []
        self.globals = {}
        self.function_table = function_table
        self.call_stack = []
        self.ip = 0

    def run(self):
        while self.ip < len(self.bytecode):
            op, arg = self.bytecode[self.ip]

            if op == "ICONST":
                self.stack.append(arg)
            elif op == "SCONST":
                self.stack.append(arg)
            elif op == "LOAD":
                if arg not in self.globals:
                    raise Exception(f"Runtime Error: Variable '{arg}' is not defined.")
                self.stack.append(self.globals[arg])
            elif op == "STORE":
                val = self.stack.pop()
                self.globals[arg] = val
            elif op == "POP":
                self.stack.pop()
            elif op == "ADD":
                b = self.stack.pop()
                a = self.stack.pop()
                if isinstance(a, str) or isinstance(b, str):
                    self.stack.append(str(a) + str(b))
                else:
                    self.stack.append(a + b)
            elif op == "SUB":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(a - b)
            elif op == "MUL":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(a * b)
            elif op == "DIV":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(int(a / b))
            elif op == "MOD":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(a % b)
            elif op == "LT":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a < b else 0)
            elif op == "GT":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a > b else 0)
            elif op == "LE":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a <= b else 0)
            elif op == "GE":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a >= b else 0)
            elif op == "EQ":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a == b else 0)
            elif op == "NE":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a != b else 0)
            elif op == "AND":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if (a and b) else 0)
            elif op == "OR":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if (a or b) else 0)
            elif op == "NOT":
                a = self.stack.pop()
                self.stack.append(1 if not a else 0)
            elif op == "NEG":
                a = self.stack.pop()
                self.stack.append(-a)
            elif op == "JMP":
                self.ip = arg
                continue
            elif op == "JZ":
                val = self.stack.pop()
                if val == 0:
                    self.ip = arg
                    continue
            elif op == "CALL":
                func_name, arg_count = arg
                if func_name not in self.function_table:
                    raise Exception(f"Runtime Error: Function '{func_name}' is not defined.")
                func_addr, param_count, _ = self.function_table[func_name]
                if arg_count != param_count:
                    raise Exception(f"Runtime Error: Function '{func_name}' expects {param_count} args, got {arg_count}.")
                args = [self.stack.pop() for _ in range(arg_count)]
                args.reverse()
                self.call_stack.append((self.ip + 1, dict(self.globals)))
                self.ip = func_addr
                for i, p in enumerate(self.function_table[func_name][2]):
                    self.globals[p] = args[i]
                continue
            elif op == "RET":
                if not self.call_stack:
                    break
                ret_ip, saved_globals = self.call_stack.pop()
                ret_val = self.stack.pop() if self.stack else 0
                self.globals = saved_globals
                self.stack.append(ret_val)
                self.ip = ret_ip
                continue
            elif op == "PRINT":
                val = self.stack.pop()
                if isinstance(val, str):
                    print(f"[Aura Output]: {val}")
                else:
                    print(f"[Aura Output]: {val}")
            elif op == "INPUT":
                user_input = input(f"[Aura Input] {arg} = ")
                try:
                    self.globals[arg] = int(user_input)
                except ValueError:
                    self.globals[arg] = user_input
            elif op == "HALT":
                break

            self.ip += 1


# =====================================================================
# 4. Program Entry — extended demos
# =====================================================================
def run_aura(source_code, label=""):
    if label:
        print(f"\n{'='*60}")
        print(f"  {label}")
        print(f"{'='*60}")

    print("\n--- Source Code ---")
    for line in source_code.strip().split('\n'):
        print(f"  {line}")

    print("\n--- Bytecode ---")
    lexer = Lexer(source_code)
    compiler = Compiler(lexer)
    bytecode, func_table = compiler.compile()

    for idx, instr in enumerate(bytecode):
        arg_str = instr[1] if instr[1] is not None else ''
        print(f"  {idx:03d}: {instr[0]:7s} {arg_str}")

    print(f"\n  Function table: { {k: (v[0], v[1]) for k, v in func_table.items()} }")

    print("\n--- Execution ---")
    vm = VirtualMachine(bytecode, func_table)
    vm.run()


if __name__ == "__main__":
    # Demo 1: Basic math with new operators
    run_aura("""
    // Test modulo and string output
    let x = 10 % 3;
    show x;
    show "Hello, Aura!";
    """, "Demo 1: Modulo & Strings")

    # Demo 2: All comparison and logical operators
    run_aura("""
    let a = 5;
    let b = 10;
    show a < b;
    show a > b;
    show a <= b;
    show a >= b;
    show a == b;
    show a != b;
    show (a < b) && (b > 0);
    show (a > b) || (b > 0);
    show !(a == b);
    """, "Demo 2: Comparisons & Logic")

    # Demo 3: Functions with parameters and return
    run_aura("""
    func fib(n) {
        if (n <= 1) {
            return n;
        }
        return fib(n - 1) + fib(n - 2);
    }

    let result = fib(10);
    show "fib(10) =";
    show result;
    """, "Demo 3: Recursive Fibonacci Function")

    # Demo 4: Multiple functions, string concatenation
    run_aura("""
    func greet(name) {
        return "Hello, " + name + "!";
    }

    func square(x) {
        return x * x;
    }

    show greet("World");
    show square(7);
    show square(3) + square(4);
    """, "Demo 4: Multi-function & String Concat")

    # Demo 5: Loop + function
    run_aura("""
    func is_even(n) {
        return n % 2 == 0;
    }

    let i = 1;
    loop (i <= 10) {
        if (is_even(i)) {
            show i;
        }
        i = i + 1;
    }
    """, "Demo 5: Loop with Function Call")

    # Demo 6: Original HW2 demo (backward compatible)
    run_aura("""
    let counter = 1;
    let limit = 4;

    loop (counter < limit) {
        show counter;
        counter = counter + 1;
    }

    if (counter == limit) {
        show 999;
    } else {
        show 0;
    }
    """, "Demo 6: Original HW2 Code (backward compatible)")
