APP := filesort
PREFIX ?= /usr/local
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)gcc
CPPFLAGS := -Iinclude -MD
CFLAGS := -Wall -Werror=vla -std=gnu89 -O2
LDLIBS := -lm

# Build with pthread support
CPPFLAGS += -pthread
LDFLAGS += -pthread

OBJS :=				\
	src/algo/heap.o		\
	src/algo/kmerge.o	\
	src/algo/pmsort.o	\
	src/main.o		\
	src/profile.o		\
	src/sort.o		\
	src/tools.o

# Be silent per default, but 'make V=1' will show all compiler calls
ifneq ($(V),1)
  Q = @
else
  Q =
endif

BUILD ?= debug
ifeq ($(BUILD),release)
  CPPFLAGS += -DNDEBUG
  CFLAGS += -s
else ifeq ($(BUILD),debug)
  CFLAGS += -g
else
  $(error Incorrect BUILD variable)
endif

all: $(APP)

$(APP): $(OBJS)
	@printf "  LD      $@\n"
	$(Q)$(LD) $(LDFLAGS) $(OBJS) -o $(APP) $(LDLIBS)

%.o: %.c
	@printf "  CC      $(*).c\n"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

clean:
	@printf "  CLEAN\n"
	$(Q)$(MAKE) -s -C test clean
	$(Q)-rm -f $(APP)
	$(Q)-rm -f $(OBJS)
	$(Q)find src/ -name '*.d' -exec rm -f {} \;
	$(Q)-rm -f cscope.*
	$(Q)-rm -f tags
	$(Q)-rm -rf doc

test: $(APP)
	@printf "  TEST\n"
	$(Q)$(MAKE) -s -C test

install: $(APP)
	@printf "  INSTALL\n"
	$(Q)install -d $(DESTDIR)$(PREFIX)/bin
	$(Q)install -m 0755 $(APP) $(DESTDIR)$(PREFIX)/bin

uninstall:
	@printf "  UNINSTALL\n"
	$(Q)$(RM) -f $(DESTDIR)$(PREFIX)/bin/$(APP)

index:
	@printf "  INDEX\n"
	$(Q)find -name '*.[ch]' > cscope.files
	$(Q)cscope -b -q
	$(Q)ctags -L cscope.files

doxy: Doxyfile
	@printf "  DOXY\n"
	$(Q)doxygen

.PHONY: all clean test install uninstall index doxy

-include $(OBJS:.o=.d)
