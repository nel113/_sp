import sys

# ---------------------------------------------------------------------
# 1. Lexer (Tokenizer)
# ---------------------------------------------------------------------
class Token:
    def __init__(self, type_, value):
        self.type = type_
        self.value = value

    def __repr__(self):
        return f"Token({self.type}, {self.value})"

# Token types
LET, SHOW, IF, ELSE, LOOP = "LET", "SHOW", "IF", "ELSE", "LOOP"
IDENT, NUM = "IDENT", "NUM"
ASSIGN, PLUS, MINUS, MUL, DIV = "ASSIGN", "PLUS", "MINUS", "MUL", "DIV"
EQ, LT = "EQ", "LT"
LPAREN, RPAREN, LBRACE, RBRACE, SEMI = "LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMI"
EOF = "EOF"

KEYWORDS = {
    "let": LET,
    "show": SHOW,
    "if": IF,
    "else": ELSE,
    "loop": LOOP
}

class Lexer:
    def __init__(self, text):
        self.text = text
        self.pos = 0
        self.current_char = self.text[0] if text else None

    def advance(self):
        self.pos += 1
        self.current_char = self.text[self.pos] if self.pos < len(self.text) else None

    def skip_whitespace(self):
        while self.current_char and self.current_char.isspace():
            self.advance()

    def identifier(self):
        result = ""
        while self.current_char and self.current_char.isalnum():
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

    def get_next_token(self):
        while self.current_char:
            if self.current_char.isspace():
                self.skip_whitespace()
                continue
            if self.current_char.isalpha():
                return self.identifier()
            if self.current_char.isdigit():
                return self.number()
            
            if self.current_char == '=':
                self.advance()
                if self.current_char == '=':
                    self.advance()
                    return Token(EQ, "==")
                return Token(ASSIGN, "=")
            
            if self.current_char == '<': self.advance(); return Token(LT, "<")
            if self.current_char == '+': self.advance(); return Token(PLUS, "+")
            if self.current_char == '-': self.advance(); return Token(MINUS, "-")
            if self.current_char == '*': self.advance(); return Token(MUL, "*")
            if self.current_char == '/': self.advance(); return Token(DIV, "/")
            if self.current_char == '(': self.advance(); return Token(LPAREN, "(")
            if self.current_char == ')': self.advance(); return Token(RPAREN, ")")
            if self.current_char == '{': self.advance(); return Token(LBRACE, "{")
            if self.current_char == '}': self.advance(); return Token(RBRACE, "}")
            if self.current_char == ';': self.advance(); return Token(SEMI, ";")
            
            raise Exception(f"Lexical Error: Unknown character '{self.current_char}'")
        
        return Token(EOF, None)

# ---------------------------------------------------------------------
# 2. Compiler (Parser -> Bytecode Generator)
# ---------------------------------------------------------------------
class Compiler:
    def __init__(self, lexer):
        self.lexer = lexer
        self.current_token = self.lexer.get_next_token()
        self.bytecode = []

    def error(self, message):
        raise Exception(f"Syntax/Compile Error: {message}")

    def consume(self, token_type):
        if self.current_token.type == token_type:
            self.current_token = self.lexer.get_next_token()
        else:
            self.error(f"Expected token {token_type}, got {self.current_token.type}")

    def emit(self, instruction):
        self.bytecode.append(instruction)

    def current_address(self):
        return len(self.bytecode)

    def patch(self, address, target_value):
        op, _ = self.bytecode[address]
        self.bytecode[address] = (op, target_value)

    def factor(self):
        token = self.current_token
        if token.type == NUM:
            self.consume(NUM)
            self.emit(("ICONST", token.value))
        elif token.type == IDENT:
            self.consume(IDENT)
            self.emit(("LOAD", token.value))
        elif token.type == LPAREN:
            self.consume(LPAREN)
            self.expr()
            self.consume(RPAREN)
        else:
            self.error(f"Unexpected factor element '{token.value}'")

    def term(self):
        self.factor()
        while self.current_token.type in (MUL, DIV):
            op_type = self.current_token.type
            self.consume(op_type)
            self.factor()
            self.emit(("MUL" if op_type == MUL else "DIV", None))

    def arith(self):
        self.term()
        while self.current_token.type in (PLUS, MINUS):
            op_type = self.current_token.type
            self.consume(op_type)
            self.term()
            self.emit(("ADD" if op_type == PLUS else "SUB", None))

    def comparison(self):
        self.arith()
        while self.current_token.type == LT:
            self.consume(LT)
            self.arith()
            self.emit(("LT", None))

    def expr(self):
        self.comparison()
        while self.current_token.type == EQ:
            self.consume(EQ)
            self.comparison()
            self.emit(("EQ", None))

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
            self.consume(ASSIGN)
            self.expr()
            self.consume(SEMI)
            self.emit(("STORE", var_name))
            
        elif self.current_token.type == SHOW:
            self.consume(SHOW)
            self.expr()
            self.consume(SEMI)
            self.emit(("PRINT", None))
            
        elif self.current_token.type == IF:
            self.consume(IF)
            self.consume(LPAREN)
            self.expr()
            self.consume(RPAREN)
            
            jz_addr = self.current_address()
            self.emit(("JZ", 0)) # Placeholder target location
            
            self.block()
            
            if self.current_token.type == ELSE:
                self.consume(ELSE)
                jmp_addr = self.current_address()
                self.emit(("JMP", 0)) # Placeholder jump past else body
                
                # If block evaluated to false, jump to the else structure location
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
            self.emit(("JZ", 0)) # Placeholder
            
            self.block()
            self.emit(("JMP", start_addr)) # Absolute loopback
            self.patch(jz_addr, self.current_address()) # Break jump mapping
            
        elif self.current_token.type == LBRACE:
            self.block()
        else:
            self.error(f"Invalid statement starter rule '{self.current_token.value}'")

    def block(self):
        self.consume(LBRACE)
        while self.current_token.type != RBRACE and self.current_token.type != EOF:
            self.statement()
        self.consume(RBRACE)

    def compile(self):
        while self.current_token.type != EOF:
            self.statement()
        self.emit(("HALT", None))
        return self.bytecode

# ---------------------------------------------------------------------
# 3. Virtual Machine (Execution Engine)
# ---------------------------------------------------------------------
class VirtualMachine:
    def __init__(self, bytecode):
        self.bytecode = bytecode
        self.stack = []
        self.globals = {}
        self.ip = 0 # Instruction Pointer

    def run(self):
        while self.ip < len(self.bytecode):
            op, arg = self.bytecode[self.ip]
            
            if op == "ICONST":
                self.stack.append(arg)
            elif op == "LOAD":
                if arg not in self.globals:
                    raise Exception(f"Runtime Error: Variable '{arg}' is not defined.")
                self.stack.append(self.globals[arg])
            elif op == "STORE":
                val = self.stack.pop()
                self.globals[arg] = val
            elif op == "ADD":
                b = self.stack.pop()
                a = self.stack.pop()
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
            elif op == "LT":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a < b else 0)
            elif op == "EQ":
                b = self.stack.pop()
                a = self.stack.pop()
                self.stack.append(1 if a == b else 0)
            elif op == "JMP":
                self.ip = arg
                continue
            elif op == "JZ":
                val = self.stack.pop()
                if val == 0:
                    self.ip = arg
                    continue
            elif op == "PRINT":
                val = self.stack.pop()
                print(f"[Aura Output]: {val}")
            elif op == "HALT":
                break
                
            self.ip += 1

# ---------------------------------------------------------------------
# 4. Program Entry Execution
# ---------------------------------------------------------------------
if __name__ == "__main__":
    aura_source_code = """
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
    """
    
    print("--- 1. Reading Aura Source Code ---")
    print(aura_source_code.strip())
    
    print("\n--- 2. Compiling to Bytecode Target ---")
    lexer = Lexer(aura_source_code)
    compiler = Compiler(lexer)
    bytecode = compiler.compile()
    
    for idx, instr in enumerate(bytecode):
        print(f"{idx:03d}: {instr[0]} {instr[1] if instr[1] is not None else ''}")
        
    print("\n--- 3. Launching Virtual Machine ---")
    vm = VirtualMachine(bytecode)
    vm.run()