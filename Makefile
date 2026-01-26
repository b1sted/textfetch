CC ?= gcc
SRCS = src/textfetch.c src/bitset.c
HEADERS = include/bitset.h

CFLAGS += -O3 -DNDEBUG -Wall -Wextra -std=c99 -Iinclude -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections -s

DEBUG_FLAGS = -Wall -Wextra -std=c99 -Iinclude -g -O0

.PHONY: all debug clean

all: bin/textfetch

debug: bin/textfetch-debug

bin/textfetch: $(SRCS) $(HEADERS) | bin
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

bin/textfetch-debug: $(SRCS) $(HEADERS) | bin
	$(CC) $(DEBUG_FLAGS) $(SRCS) -o $@

bin:
	mkdir -p bin

clean:
	rm -rf bin