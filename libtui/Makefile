# libtui Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -I./include -D_GNU_SOURCE -I$(HOME)/.nix-profile/include
LDFLAGS = -shared -L$(HOME)/.nix-profile/lib
LIBS = -lncurses

# Source files
SRCS = src/tui.c \
       src/theme.c \
       src/window.c \
       src/pane.c \
       src/mouse.c \
       src/util.c

# Object files
OBJS = $(SRCS:.c=.o)

# Target library
TARGET = libtui.so
STATIC_TARGET = libtui.a

# Installation directories
PREFIX ?= /usr/local
INCLUDEDIR = $(PREFIX)/include
LIBDIR = $(PREFIX)/lib

.PHONY: all clean install uninstall

all: $(STATIC_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(STATIC_TARGET): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(STATIC_TARGET)

install: $(TARGET) $(STATIC_TARGET)
	install -d $(DESTDIR)$(INCLUDEDIR)/tui
	install -m 644 include/tui.h $(DESTDIR)$(INCLUDEDIR)/tui/
	install -d $(DESTDIR)$(LIBDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(LIBDIR)/
	install -m 644 $(STATIC_TARGET) $(DESTDIR)$(LIBDIR)/

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/tui/tui.h
	rm -f $(DESTDIR)$(LIBDIR)/$(TARGET)
	rm -f $(DESTDIR)$(LIBDIR)/$(STATIC_TARGET)
	rmdir $(DESTDIR)$(INCLUDEDIR)/tui 2>/dev/null || true