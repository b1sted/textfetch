# SPDX-License-Identifier: MIT

CC ?= cc

TEST_CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g -O1 \
               -D_POSIX_C_SOURCE=200809L
TEST_CPPFLAGS := -I../include -Isupport -Isupport/stubs
TEST_LDFLAGS :=

SANITIZE ?= 1
ifeq ($(SANITIZE),1)
TEST_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
TEST_LDFLAGS += -fsanitize=address,undefined
endif

TEST_BINDIR := ../bin/tests

CORE_TEST_DIR := core
PLATFORM_TEST_DIR := platform
SUPPORT_DIR := support

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
ASAN_OPTIONS := detect_leaks=1:abort_on_error=1
else
# Apple's AddressSanitizer runtime does not implement leak detection.
TEST_NAMES += test_macos_hardware
# Expose BSD/POSIX extensions such as _SC_NPROCESSORS_CONF on Darwin.
TEST_CFLAGS += -D_DARWIN_C_SOURCE
# The terminal test exercises the CoreFoundation locale fallback.
TERMINAL_TEST_LDFLAGS := -framework CoreFoundation
ASAN_OPTIONS := abort_on_error=1
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
		ASAN_OPTIONS=$(ASAN_OPTIONS) \
		UBSAN_OPTIONS=halt_on_error=1 \
		$$test_binary $(TEXTFETCH_BIN); \
	done

$(TEST_BINDIR)/test_bitset: $(CORE_TEST_DIR)/test_bitset.c \
		../src/core/bitset.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_hashtable: $(CORE_TEST_DIR)/test_hashtable.c \
		../src/core/hashtable.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_binary_trees: $(CORE_TEST_DIR)/test_binary_trees.c \
		../src/core/binary_trees.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_config: $(CORE_TEST_DIR)/test_config.c \
		../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_sys_utils: $(CORE_TEST_DIR)/test_sys_utils.c \
		../src/core/sys_utils.c ../src/core/binary_trees.c \
		../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_ui: $(CORE_TEST_DIR)/test_ui.c ../src/core/ui.c \
		../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_system: $(CORE_TEST_DIR)/test_system.c \
		$(SUPPORT_DIR)/test_stubs.c \
		../src/core/system.c ../src/core/hardware.c \
		../src/core/ui.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_terminal: $(CORE_TEST_DIR)/test_terminal.c \
		../src/platform/posix/terminal.c ../src/core/ui.c \
		../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(TERMINAL_TEST_LDFLAGS)

$(TEST_BINDIR)/test_capture: $(PLATFORM_TEST_DIR)/test_capture.c \
		../src/platform/linux-common/capture.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_posix_system: $(PLATFORM_TEST_DIR)/test_posix_system.c \
		../src/platform/posix/system.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_android_system: $(PLATFORM_TEST_DIR)/test_android_system.c \
		../src/platform/android/system.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $(PLATFORM_TEST_DIR)/test_android_system.c \
		../src/core/config.c -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_linux_system: $(PLATFORM_TEST_DIR)/test_linux_system.c \
		../src/platform/linux-common/system.c \
		../src/core/sys_utils.c ../src/core/binary_trees.c \
		../src/core/bitset.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_linux_terminal: $(PLATFORM_TEST_DIR)/test_linux_terminal.c \
		../src/platform/posix/terminal.c \
		../src/platform/linux-common/terminal.c \
		../src/platform/linux-common/capture.c \
		../src/core/ui.c ../src/core/config.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) \
		$(filter-out ../src/platform/linux-common/terminal.c,$^) \
		-o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_linux_hardware: $(PLATFORM_TEST_DIR)/test_linux_hardware.c \
		$(CORE_BASE)
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

ifeq ($(UNAME_S),Darwin)
$(TEST_BINDIR)/test_macos_hardware: $(PLATFORM_TEST_DIR)/test_macos_hardware.c \
		../src/platform/posix/hardware.c $(CORE_BASE)
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ \
		$(TEST_LDFLAGS) -framework IOKit -framework CoreFoundation
endif

$(TEST_BINDIR)/test_linux_common_hardware: \
		$(PLATFORM_TEST_DIR)/test_linux_common_hardware.c \
		../src/platform/posix/hardware.c $(CORE_BASE)
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(TEST_BINDIR)/test_cli: $(CORE_TEST_DIR)/test_cli.c
	@mkdir -p $(TEST_BINDIR)
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $^ -o $@ $(TEST_LDFLAGS)

clean:
	rm -rf $(TEST_BINDIR)
