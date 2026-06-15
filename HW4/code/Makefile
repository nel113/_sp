# Build all examples
CC     ?= gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -O2 -g

ALL = pipeline epoll_echo_server raw_syscall_signal

all: $(ALL)

pipeline: pipeline.o
	$(CC) $(CFLAGS) -o $@ $<

epoll_echo_server: epoll_echo_server.o
	$(CC) $(CFLAGS) -o $@ $<

raw_syscall_signal: raw_syscall_signal.o
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(ALL) *.o
