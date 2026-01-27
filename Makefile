CC          ?= gcc
CPPFLAGS    += -Iinclude -MMD -MP
CFLAGS      += -Wall -Wextra -std=c99
LDFLAGS     += 

BINDIR      = bin
OBJDIR      = obj
SRCDIR      = src
TARGET      = $(BINDIR)/textfetch
TARGET_DEV  = $(BINDIR)/textfetch-dev

SRCS        = $(wildcard $(SRCDIR)/*.c)

OBJS_REL    = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/release/%.o)
OBJS_DEV    = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/dev/%.o)

DEPS        = $(OBJS_REL:.o=.d) $(OBJS_DEV:.o=.d)

REL_CFLAGS  = -O3 -DNDEBUG -ffunction-sections -fdata-sections
REL_LDFLAGS = -Wl,--gc-sections -s

DEV_CFLAGS  = -g -O0 -DDEBUG

.PHONY: all dev clean

all: $(TARGET)

dev: $(TARGET_DEV)

$(TARGET): $(OBJS_REL) | $(BINDIR)
	$(CC) $(CFLAGS) $(REL_CFLAGS) $^ -o $@ $(LDFLAGS) $(REL_LDFLAGS)

$(TARGET_DEV): $(OBJS_DEV) | $(BINDIR)
	$(CC) $(CFLAGS) $(DEV_CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJDIR)/release/%.o: $(SRCDIR)/%.c | $(OBJDIR)/release
	$(CC) $(CPPFLAGS) $(CFLAGS) $(REL_CFLAGS) -c $< -o $@

$(OBJDIR)/dev/%.o: $(SRCDIR)/%.c | $(OBJDIR)/dev
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEV_CFLAGS) -c $< -o $@

$(BINDIR) $(OBJDIR)/release $(OBJDIR)/dev:
	mkdir -p $@

-include $(DEPS)

clean:
	rm -rf $(BINDIR) $(OBJDIR)