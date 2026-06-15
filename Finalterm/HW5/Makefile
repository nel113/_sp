CC      = gcc
CFLAGS  = -pthread -Wall -Wextra -O2

BANK     = bank/bank
PC       = producer_consumer/pc
PHIL     = dining_philosophers/philosophers

.PHONY: all clean run

all: $(BANK) $(PC) $(PHIL)

$(BANK): bank/bank.c
	$(CC) $(CFLAGS) $< -o $@

$(PC): producer_consumer/pc.c
	$(CC) $(CFLAGS) $< -o $@

$(PHIL): dining_philosophers/philosophers.c
	$(CC) $(CFLAGS) $< -o $@

run: all
	@echo
	$(BANK)
	@echo
	$(PC)
	@echo
	$(PHIL)

clean:
	rm -f $(BANK) $(PC) $(PHIL)
