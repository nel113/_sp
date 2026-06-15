#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ---------------------------------------------------------------------
// 1. Lexer (Tokenizer) - Scans source code and extracts tokens
// ---------------------------------------------------------------------
char *src, *p;
int token;
char token_str[100];

enum {
    Id, Num, While, If, Else, Func, 
    Assign, Add, Sub, Mul, Div, Eq, Neq, Lt, Gt, 
    Lpar, Rpar, Lbrace, Rbrace, Semi, Eof
};

void next() {
    while (*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') p++;

    if (*p == '\0') { token = Eof; return; }

    if (isalpha(*p)) {
        char *s = token_str;
        while (isalnum(*p)) *s++ = *p++;
        *s = '\0';
        
        if (strcmp(token_str, "while") == 0) token = While;
        else if (strcmp(token_str, "if") == 0) token = If;
        else if (strcmp(token_str, "else") == 0) token = Else;
        else if (strcmp(token_str, "func") == 0) token = Func;
        else token = Id;
        return;
    }

    if (isdigit(*p)) {
        char *s = token_str;
        while (isdigit(*p)) *s++ = *p++;
        *s = '\0';
        token = Num;
        return;
    }

    switch (*p) {
        case '=': if (p[1] == '=') { p+=2; token = Eq; } else { p++; token = Assign; } return;
        case '!': if (p[1] == '=') { p+=2; token = Neq; } return;
        case '<': p++; token = Lt; return;
        case '>': p++; token = Gt; return;
        case '+': p++; token = Add; return;
        case '-': p++; token = Sub; return;
        case '*': p++; token = Mul; return;
        case '/': p++; token = Div; return;
        case '(': p++; token = Lpar; return;
        case ')': p++; token = Rpar; return;
        case '{': p++; token = Lbrace; return;
        case '}': p++; token = Rbrace; return;
        case ';': p++; token = Semi; return;
    }
    printf("Syntax Error: Unknown character '%c'\n", *p);
    exit(1);
}

void match(int expected) {
    if (token == expected) next();
    else {
        printf("Syntax Error: Unexpected token '%s'\n", token_str);
        exit(1);
    }
}

// ---------------------------------------------------------------------
// 2. Code Generator Utilities
// ---------------------------------------------------------------------
int label_counter = 0;
int next_label() { return label_counter++; }

void emit(const char *inst) { printf("    %s\n", inst); }
void emit_label(int l) { printf("L%d:\n", l); }
void emit_fmt(const char *fmt, const char *arg) { printf("    "); printf(fmt, arg); printf("\n"); }

// ---------------------------------------------------------------------
// 3. Parser (Recursive Descent)
// ---------------------------------------------------------------------
void expr();
void stmt();
void block();

// EXP = ID | NUM | EXP OP EXP
void factor() {
    if (token == Id) {
        emit_fmt("PUSH %s", token_str);
        match(Id);
    } else if (token == Num) {
        emit_fmt("PUSH %s", token_str);
        match(Num);
    } else if (token == Lpar) {
        match(Lpar); expr(); match(Rpar);
    }
}

void expr() {
    factor();
    while (token == Add || token == Sub || token == Lt || token == Gt || token == Eq) {
        int op = token;
        match(token);
        factor();
        if (op == Add) emit("ADD");
        else if (op == Sub) emit("SUB");
        else if (op == Lt) emit("CMP_LT");
        else if (op == Gt) emit("CMP_GT");
        else if (op == Eq) emit("CMP_EQ");
    }
}

// STMT = WHILE | BLOCK | ASSIGN
void while_stmt() {
    int l_start = next_label();
    int l_end = next_label();

    emit_label(l_start);      // 1. Mark the start of the loop
    match(While);
    match(Lpar);
    expr();                   // 2. Evaluate the condition
    match(Rpar);
    
    char jz_inst[20];
    sprintf(jz_inst, "JZ L%d", l_end);
    emit(jz_inst);            // 3. Jump to end if condition is false (0)

    stmt();                   // 4. Execute the loop body

    char jmp_inst[20];
    sprintf(jmp_inst, "JMP L%d", l_start);
    emit(jmp_inst);           // 5. Jump back to start to re-evaluate
    
    emit_label(l_end);        // 6. Mark the end of the loop
}

void assign_stmt() {
    char var_name[100];
    strcpy(var_name, token_str);
    match(Id);
    match(Assign);
    expr();
    match(Semi);
    emit_fmt("STORE %s", var_name);
}

void stmt() {
    if (token == While) {
        while_stmt();
    } else if (token == Lbrace) {
        block();
    } else if (token == Id) {
        assign_stmt();
    }
}

void block() {
    match(Lbrace);
    while (token != Rbrace && token != Eof) {
        stmt();
    }
    match(Rbrace);
}

void func_decl() {
    match(Func);
    char func_name[100];
    strcpy(func_name, token_str);
    match(Id);
    match(Lpar); match(Rpar);
    
    printf("\n%s:\n", func_name);
    emit("PUSH BP");          // Function Prologue: Save Caller's Base Pointer
    emit("MOV BP, SP");       // Set up new Base Pointer for current frame
    
    block();
    
    emit("MOV SP, BP");       // Function Epilogue: Clear local variables
    emit("POP BP");           // Restore Caller's Base Pointer
    emit("RET");              // Return to Caller
}

void program() {
    while (token != Eof) {
        if (token == Func) func_decl();
        else stmt();
    }
}

// ---------------------------------------------------------------------
// 4. Main Entry Point
// ---------------------------------------------------------------------
int main() {
    char source_code[] = 
        "func main() {\n"
        "    i = 0;\n"
        "    while (i < 10) {\n"
        "        i = i + 1;\n"
        "    }\n"
        "}\n";

    printf("--- Compiling Source Code ---\n%s\n", source_code);
    printf("--- Generated Assembly ---\n");

    src = source_code;
    p = src;
    
    next();    // Load first token
    program(); // Start parsing

    return 0;
}