CC ?= gcc
CFLAGS += -Wall -Wextra -g -std=c99

.PHONY: all clean

all: bin/textfetch

bin/textfetch: src/textfetch.c | bin
	$(CC) $(CFLAGS) src/textfetch.c -o bin/textfetch

bin:
	mkdir -p bin

clean:
	@echo "Cleaning project."
	rm -rf bin
	@echo "Done."