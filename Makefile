CC          ?= gcc
override CPPFLAGS += -Iinclude -MMD -MP
override CFLAGS   += -Wall -Wextra -std=c99
LDFLAGS     += 

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

BINDIR        := bin
OBJDIR        := obj
SRCDIR        := src
TARGET        := $(BINDIR)/textfetch
TARGET_DEV    := $(BINDIR)/textfetch-dev
TARGET_LEAKS  := $(BINDIR)/textfetch-leaks

CORE_SRCS     := $(wildcard $(SRCDIR)/core/*.c)
PLATFORM_SRCS := $(foreach dir,$(PLATFORM_SUBDIRS),$(wildcard $(SRCDIR)/platform/$(dir)/*.c))
SRCS          := $(CORE_SRCS) $(PLATFORM_SRCS)

OBJS_REL      := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/release/%.o, $(SRCS))
OBJS_DEV      := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/dev/%.o, $(SRCS))
OBJS_LEAKS    := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/leaks/%.o, $(SRCS))

DEPS          := $(OBJS_REL:.o=.d) $(OBJS_DEV:.o=.d) $(OBJS_LEAKS:.o=.d)

.PHONY: all dev leaks clean

all: $(TARGET)

dev: $(TARGET_DEV)

leaks: $(TARGET_LEAKS)

$(TARGET): $(OBJS_REL) | $(BINDIR)
	$(CC) $(CFLAGS) $(REL_CFLAGS) $^ -o $@ $(LDFLAGS) $(REL_LDFLAGS)

$(TARGET_DEV): $(OBJS_DEV) | $(BINDIR)
	$(CC) $(CFLAGS) $(DEV_CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_LEAKS): $(OBJS_LEAKS) | $(BINDIR)
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

$(BINDIR):
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(BINDIR) $(OBJDIR)