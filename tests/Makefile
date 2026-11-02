# SPDX-License-Identifier: MIT

CC ?= cc

TEST_CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g -O1 \
               -D_POSIX_C_SOURCE=200809L
TEST_CPPFLAGS := -I../include -Iandroid_stubs
TEST_LDFLAGS :=

SANITIZE ?= 1
ifeq ($(SANITIZE),1)
TEST_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
TEST_LDFLAGS += -fsanitize=address,undefined
endif

TEST_BINDIR := ../bin/tests
TEST_NAMES := \
	test_bitset \
	test_hashtable \
	test_binary_trees \
	test_config \
	test_sys_utils \
	test_ui \
	test_system \
	test_terminal \
	test_posix_system \
	test_android_system \
	test_cli

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
TEST_NAMES += \
	test_capture \
	test_linux_system \
	test_linux_terminal \
	test_linux_hardware \
	test_linux_common_hardware
endif

TEST_BINS := $(addprefix $(TEST_BINDIR)/,$(TEST_NAMES))

CORE_BASE := \
	../src/core/config.c \
	../src/core/sys_utils.c \
	../src/core/binary_trees.c \
	../src/core/bitset.c \
	../src/core/hashtable.c \
	../src/core/ui.c

.PHONY: all test clean

all: $(TEST_BINS)

test: $(TEST_BINS)
	@set -e; \
	for test_binary in $(TEST_BINS); do \
		echo "== $$test_binary"; \
		ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
		UBSAN_OPTIONS=halt_on_error=1 \
		$$test_binary $(TEXTFETCH_BIN); \
	done

$(TEST_BINDIR)/test_bitset: test_bitset.c ../src/core/bitset.c \
		../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_hashtable: test_hashtable.c ../src/core/hashtable.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_binary_trees: test_binary_trees.c \
		../src/core/binary_trees.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_config: test_config.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_sys_utils: test_sys_utils.c \
		../src/core/sys_utils.c ../src/core/binary_trees.c \
		../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_ui: test_ui.c ../src/core/ui.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_system: test_system.c test_stubs.c \
		../src/core/system.c ../src/core/hardware.c \
		../src/core/ui.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_terminal: test_terminal.c \
		../src/platform/posix/terminal.c ../src/core/ui.c \
		../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_capture: test_capture.c \
		../src/platform/linux-common/capture.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_posix_system: test_posix_system.c \
		../src/platform/posix/system.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_android_system: test_android_system.c \
		../src/platform/android/system.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) test_android_system.c \
		../src/core/config.c -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_linux_system: test_linux_system.c \
		../src/platform/linux-common/system.c \
		../src/core/sys_utils.c ../src/core/binary_trees.c \
		../src/core/bitset.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_linux_terminal: test_linux_terminal.c \
		../src/platform/posix/terminal.c \
		../src/platform/linux-common/terminal.c \
		../src/platform/linux-common/capture.c \
		../src/core/ui.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_linux_hardware: test_linux_hardware.c $(CORE_BASE)
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_linux_common_hardware: test_linux_common_hardware.c \
		../src/platform/posix/hardware.c $(CORE_BASE)
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_cli: test_cli.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

clean:
	rm -rf $(TEST_BINDIR)
