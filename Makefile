CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -fPIE
CFLAGS_TUI = $(CFLAGS) -I$(LIBTUI_DIR)/include -Ilibsacd -I$(HOME)/.nix-profile/include
LDFLAGS = -lncurses -L$(HOME)/.nix-profile/lib

TARGET = sacd-lab
TARGET_TUI = sacd-lab-tui
SOURCES = main.c ui.c window.c keys.c commands.c browser_v2.c sacd_api_libsacd.c sacd_api_impl.c
SOURCES_TUI = main_tui.c sacd_tui_adapter.c sacd_api_libsacd.c sacd_api_impl.c sacd_extract_lib_simple.c
OBJECTS = $(SOURCES:.c=.o)
OBJECTS_TUI = $(SOURCES_TUI:.c=.o)
HEADERS = ui.h window.h keys.h commands.h browser_v2.h sacd_api.h sacd_tui_adapter.h

LIBTUI_DIR = libtui
LIBTUI_LIB = $(LIBTUI_DIR)/libtui.a

# SACD Library
LIBSACD_DIR = libsacd
LIBSACD_LIB = $(LIBSACD_DIR)/libsacd.a

.PHONY: all clean libtui libsacd test test-libsacd

all: $(TARGET) $(TARGET_TUI)

libsacd:
	$(MAKE) -C $(LIBSACD_DIR) static

test: test_sacd_api test_extract test_real_extract

test-libsacd: test_libsacd
	
test_libsacd: test_libsacd.o $(LIBSACD_LIB)
	$(CC) test_libsacd.o -o test_libsacd $(LIBSACD_LIB) -lpthread

test_libsacd.o: test_libsacd.c
	$(CC) $(CFLAGS) -Ilibsacd -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(TARGET_TUI): $(OBJECTS_TUI) $(LIBTUI_LIB) $(LIBSACD_LIB)
	$(CC) $(OBJECTS_TUI) -o $(TARGET_TUI) $(LIBTUI_LIB) $(LIBSACD_LIB) -L$(HOME)/.nix-profile/lib /usr/lib/x86_64-linux-gnu/libncurses.so.6 /usr/lib/x86_64-linux-gnu/libtinfo.so.6 -lpthread

$(LIBTUI_LIB): libtui

libtui:
	$(MAKE) -C $(LIBTUI_DIR)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Special rules for TUI objects
main_tui.o: main_tui.c $(HEADERS)
	$(CC) $(CFLAGS_TUI) -c $< -o $@

sacd_tui_adapter.o: sacd_tui_adapter.c $(HEADERS)
	$(CC) $(CFLAGS_TUI) -c $< -o $@

sacd_api_libsacd.o: sacd_api_libsacd.c sacd_api.h
	$(CC) $(CFLAGS_TUI) -c $< -o $@

sacd_api_impl.o: sacd_api_impl.c
	$(CC) $(CFLAGS_TUI) -c $< -o $@

sacd_extract_lib_simple.o: sacd_extract_lib_simple.c sacd_extract_api.h
	$(CC) $(CFLAGS_TUI) -c $< -o $@

sacd_extract_command.o: sacd_extract_command.c sacd_extract_api.h
	$(CC) $(CFLAGS) -c $< -o $@

test_sacd_api: test_sacd_api.o sacd_api.o
	$(CC) test_sacd_api.o sacd_api.o -o test_sacd_api

test_sacd_api.o: test_sacd_api.c sacd_api.h
	$(CC) $(CFLAGS) -c $< -o $@

test_extract: test_extract.o sacd_api.o sacd_extract_mock.o
	$(CC) test_extract.o sacd_api.o sacd_extract_mock.o -o test_extract -lpthread

test_real_extract: test_real_extract.o sacd_api.o sacd_extract_standalone.o
	$(CC) test_real_extract.o sacd_api.o sacd_extract_standalone.o -o test_real_extract -lpthread

test_extract.o: test_extract.c sacd_api.h sacd_extract_api.h
	$(CC) $(CFLAGS) -c $< -o $@

test_real_extract.o: test_real_extract.c sacd_api.h sacd_extract_api.h
	$(CC) $(CFLAGS) -c $< -o $@

sacd_extract_api.o: sacd_extract_api.c sacd_extract_api.h
	$(CC) $(CFLAGS) $(SACD_INCLUDES) -c $< -o $@

sacd_extract_simple.o: sacd_extract_simple.c sacd_extract_api.h
	$(CC) $(CFLAGS) $(SACD_INCLUDES) -c $< -o $@

sacd_extract_standalone.o: sacd_extract_standalone.c sacd_extract_api.h
	$(CC) $(CFLAGS) -c $< -o $@

sacd_extract_mock.o: sacd_extract_mock.c sacd_extract_api.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET) $(TARGET_TUI) test_sacd_api test_extract
	$(MAKE) -C $(LIBTUI_DIR) clean

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)