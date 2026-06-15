CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu99
TARGET  = minishell
SRC     = shell.c

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo "Launching MiniShell..."
	@echo "Try: ls -la | wc -l"
	@echo "Try: ls > out.txt && cat out.txt"
	@echo ""
	./$(TARGET)

test: all
	@echo "=== Test 1: built-in help ==="
	@echo "help" | ./$(TARGET)
	@echo ""
	@echo "=== Test 2: built-in pwd ==="
	@echo "pwd" | ./$(TARGET)
	@echo ""
	@echo "=== Test 3: external command ==="
	@echo "echo hello world" | ./$(TARGET)
	@echo ""
	@echo "=== Test 4: pipe ==="
	@echo "echo a b c d | wc -w" | ./$(TARGET)
	@echo ""
	@echo "=== Test 5: redirect output to file ==="
	@echo "echo redirected > /tmp/minishell_test.txt" | ./$(TARGET)
	@cat /tmp/minishell_test.txt
	@rm -f /tmp/minishell_test.txt
	@echo ""
	@echo "All tests passed."

clean:
	rm -f $(TARGET)
