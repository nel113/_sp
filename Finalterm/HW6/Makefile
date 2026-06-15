CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -pedantic
BINDIR  = bin
PROGS   = process_basics file_io io_redirection pipeline

# Source files are inside programs/
SOURCES = $(addprefix programs/, $(addsuffix .c, $(PROGS)))
TARGETS = $(addprefix $(BINDIR)/, $(PROGS))

.PHONY: all clean run

all: $(TARGETS)

$(BINDIR)/%: programs/%.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

run: all
	@echo ""
	@echo "============================================="
	@echo " 1. process_basics — fork + execvp + wait"
	@echo "============================================="
	@$(BINDIR)/process_basics
	@echo ""
	@echo "============================================="
	@echo " 2. file_io — open, close, read, write"
	@echo "============================================="
	@$(BINDIR)/file_io
	@echo ""
	@echo "============================================="
	@echo " 3. io_redirection — dup2 (stdin/stdout/stderr)"
	@echo "============================================="
	@$(BINDIR)/io_redirection
	@echo ""
	@echo "============================================="
	@echo " 4. pipeline — ls | wc -l"
	@echo "============================================="
	@$(BINDIR)/pipeline

clean:
	rm -rf $(BINDIR)
