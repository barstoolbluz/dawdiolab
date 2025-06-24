# SACD Library Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -fPIC -D_GNU_SOURCE
LDFLAGS = -lpthread

# Library name and version
LIBNAME = libsacd
VERSION = 1.0.0
MAJOR = 1

# Source files
SOURCES = sacd_disc.c sacd_utils.c sacd_formats.c sacd_dst.c sacd_extractor.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = sacd_lib.h sacd_internal.h

# Targets
STATIC_LIB = $(LIBNAME).a
SHARED_LIB = $(LIBNAME).so.$(VERSION)
SHARED_LIB_LINK = $(LIBNAME).so.$(MAJOR)
SHARED_LIB_SIMPLE = $(LIBNAME).so

.PHONY: all clean install shared static

all: static shared

static: $(STATIC_LIB)

shared: $(SHARED_LIB)

# Static library
$(STATIC_LIB): $(OBJECTS)
	ar rcs $@ $^

# Shared library
$(SHARED_LIB): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SHARED_LIB_LINK) -o $@ $^ $(LDFLAGS)
	ln -sf $(SHARED_LIB) $(SHARED_LIB_LINK)
	ln -sf $(SHARED_LIB) $(SHARED_LIB_SIMPLE)

# Object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB) $(SHARED_LIB_LINK) $(SHARED_LIB_SIMPLE)

# Debug build
debug: CFLAGS += -DSACD_DEBUG -O0
debug: all

# Install (optional)
install: all
	mkdir -p /usr/local/lib /usr/local/include
	cp $(STATIC_LIB) $(SHARED_LIB) /usr/local/lib/
	cp sacd_lib.h /usr/local/include/
	ldconfig

# Test compilation
test: all
	$(CC) $(CFLAGS) -I. -L. -o test_sacd test_sacd.c -lsacd $(LDFLAGS)

.SUFFIXES: .c .o