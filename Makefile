CC          ?= gcc
override CPPFLAGS += -Iinclude -MMD -MP
override CFLAGS   += -Wall -Wextra -std=c99
LDFLAGS     += 

PREFIX      ?= /usr/local
BINDIR      ?= $(PREFIX)/bin
MANDIR      ?= $(PREFIX)/share/man/man1

DEV_CFLAGS  := -g -O0 -DDEBUG
REL_CFLAGS  := -O3 -DNDEBUG -ffunction-sections -fdata-sections

LEAKS_CFLAGS  := -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined
LEAKS_LDFLAGS := -fsanitize=address -fsanitize=undefined

UNAME_S := $(shell uname -s)
UNAME_O := $(shell uname -o 2>/dev/null)

PLATFORM_SUBDIRS := posix

ifeq ($(UNAME_S), Darwin)
    PLATFORM_SUBDIRS += macos
    DEV_CFLAGS       += -gdwarf-4
    REL_LDFLAGS      := -Wl,-dead_strip,-source_version,1.0
    override LDFLAGS += -framework IOKit -framework CoreFoundation
else ifneq (,$(filter $(UNAME_S),Linux Android))
    PLATFORM_SUBDIRS += linux-common
    REL_LDFLAGS      := -Wl,--gc-sections -s
    
    ifeq ($(UNAME_O), Android)
        PLATFORM_SUBDIRS += android
        override LDFLAGS += -lEGL -lGLESv2
    else
        PLATFORM_SUBDIRS += linux
    endif
else
    $(error Platform $(UNAME_S) not supported)
endif

BUILD_BINDIR  := bin
OBJDIR        := obj
SRCDIR        := src
TARGET        := $(BUILD_BINDIR)/textfetch
TARGET_DEV    := $(BUILD_BINDIR)/textfetch-dev
TARGET_LEAKS  := $(BUILD_BINDIR)/textfetch-leaks

CORE_SRCS     := $(wildcard $(SRCDIR)/core/*.c)
PLATFORM_SRCS := $(foreach dir,$(PLATFORM_SUBDIRS),$(wildcard $(SRCDIR)/platform/$(dir)/*.c))
SRCS          := $(CORE_SRCS) $(PLATFORM_SRCS)

LINT_SRCS     := $(CORE_SRCS) $(wildcard $(SRCDIR)/platform/posix/*.c)
LINT_TESTS    := $(filter-out tests/test_capture.c tests/test_linux_%.c,$(wildcard tests/test_*.c))

ifeq ($(UNAME_S), Darwin)
    LINT_SRCS += $(wildcard $(SRCDIR)/platform/macos/*.c)
else ifneq (,$(filter $(UNAME_S),Linux Android))
    LINT_SRCS += $(wildcard $(SRCDIR)/platform/linux-common/*.c)
    LINT_TESTS += tests/test_capture.c $(wildcard tests/test_linux_*.c)

    ifneq ($(UNAME_O), Android)
        LINT_SRCS += $(wildcard $(SRCDIR)/platform/linux/*.c)
    endif
endif

OBJS_REL      := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/release/%.o, $(SRCS))
OBJS_DEV      := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/dev/%.o, $(SRCS))
OBJS_LEAKS    := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/leaks/%.o, $(SRCS))

DEPS          := $(OBJS_REL:.o=.d) $(OBJS_DEV:.o=.d) $(OBJS_LEAKS:.o=.d)

.PHONY: all dev leaks test lint clean install uninstall

all: $(TARGET)

dev: $(TARGET_DEV)

leaks: $(TARGET_LEAKS)

test: $(TARGET)
	$(MAKE) -C tests test TEXTFETCH_BIN="$(abspath $(TARGET))"

lint:
	@command -v clang-tidy >/dev/null 2>&1 || { \
		echo "error: clang-tidy is required for linting" >&2; exit 1; \
	}
	@set -e; \
	for file in $(LINT_SRCS); do \
		echo "clang-tidy $$file"; \
		clang-tidy --quiet "$$file" -- -Iinclude -std=c99; \
	done
	@set -e; \
	for file in $(LINT_TESTS); do \
		echo "clang-tidy $$file"; \
		clang-tidy --quiet "$$file" -- -Iinclude -Itests \
			-Itests/android_stubs -std=c99 -D_POSIX_C_SOURCE=200809L; \
	done
	@command -v cppcheck >/dev/null 2>&1 || { \
		echo "error: cppcheck is required for linting" >&2; exit 1; \
	}
	cppcheck --enable=warning,performance,portability --error-exitcode=1 \
		--std=c99 --suppress=missingIncludeSystem --suppress=unusedFunction \
		--suppress=normalCheckLevelMaxBranches -Iinclude $(LINT_SRCS)

$(TARGET): $(OBJS_REL) | $(BUILD_BINDIR)
	$(CC) $(CFLAGS) $(REL_CFLAGS) $^ -o $@ $(LDFLAGS) $(REL_LDFLAGS)

$(TARGET_DEV): $(OBJS_DEV) | $(BUILD_BINDIR)
	$(CC) $(CFLAGS) $(DEV_CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_LEAKS): $(OBJS_LEAKS) | $(BUILD_BINDIR)
	$(CC) $(CFLAGS) $(LEAKS_CFLAGS) $^ -o $@ $(LDFLAGS) $(LEAKS_LDFLAGS)

$(OBJDIR)/release/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(REL_CFLAGS) -c $< -o $@

$(OBJDIR)/dev/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEV_CFLAGS) -c $< -o $@

$(OBJDIR)/leaks/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LEAKS_CFLAGS) -c $< -o $@

$(BUILD_BINDIR):
	mkdir -p $@

-include $(DEPS)

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/textfetch"
	@if [ -f "man/textfetch.1" ]; then \
		install -d "$(DESTDIR)$(MANDIR)"; \
		install -m 644 "man/textfetch.1" "$(DESTDIR)$(MANDIR)/textfetch.1"; \
	fi

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/textfetch"
	rm -f "$(DESTDIR)$(MANDIR)/textfetch.1"

clean:
	rm -rf $(BUILD_BINDIR) $(OBJDIR)
