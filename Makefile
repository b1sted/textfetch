DEV_CFLAGS  = -g -O0 -DDEBUG

UNAME_S := $(shell uname -s)

ifneq (,$(filter $(UNAME_S),Linux Android))
    PLATFORM_DIR := linux
    REL_LDFLAGS  := -Wl,--gc-sections -s
else ifeq ($(UNAME_S), Darwin)
    PLATFORM_DIR := macos
	DEV_CFLAGS   += -gdwarf-4
    override REL_LDFLAGS := -Wl,-dead_strip,-source_version,1.0
	override LDFLAGS	 += -framework IOKit -framework CoreFoundation
else
    $(error Platform $(UNAME_S) not supported)
endif

CC          ?= gcc
override CPPFLAGS += -Iinclude -MMD -MP
override CFLAGS   += -Wall -Wextra -std=c99
LDFLAGS     += 

BINDIR      = bin
OBJDIR      = obj
SRCDIR      = src
TARGET      = $(BINDIR)/textfetch
TARGET_DEV  = $(BINDIR)/textfetch-dev

CORE_SRCS     = $(wildcard $(SRCDIR)/core/*.c)
PLATFORM_SRCS = $(wildcard $(SRCDIR)/platform/$(PLATFORM_DIR)/*.c)
SRCS          = $(CORE_SRCS) $(PLATFORM_SRCS)

OBJS_REL    = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/release/%.o, $(SRCS))
OBJS_DEV    = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/dev/%.o, $(SRCS))

DEPS        = $(OBJS_REL:.o=.d) $(OBJS_DEV:.o=.d)

REL_CFLAGS  = -O3 -DNDEBUG -ffunction-sections -fdata-sections

.PHONY: all dev clean

all: $(TARGET)

dev: $(TARGET_DEV)

$(TARGET): $(OBJS_REL) | $(BINDIR)
	$(CC) $(CFLAGS) $(REL_CFLAGS) $^ -o $@ $(LDFLAGS) $(REL_LDFLAGS)

$(TARGET_DEV): $(OBJS_DEV) | $(BINDIR)
	$(CC) $(CFLAGS) $(DEV_CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJDIR)/release/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(REL_CFLAGS) -c $< -o $@

$(OBJDIR)/dev/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEV_CFLAGS) -c $< -o $@

$(BINDIR):
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(BINDIR) $(OBJDIR)