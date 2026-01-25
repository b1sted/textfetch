CC ?= gcc
CFLAGS += -Wall -Wextra -g -std=c99 -Iinclude

.PHONY: all clean

all: bin/textfetch

bin/textfetch: src/textfetch.c src/bitset.c include/bitset.h | bin
	$(CC) $(CFLAGS) src/textfetch.c src/bitset.c -o bin/textfetch

bin:
	mkdir -p bin

clean:
	@echo "Cleaning project."
	rm -rf bin
	@echo "Done."