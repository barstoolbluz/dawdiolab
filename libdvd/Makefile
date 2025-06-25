CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -fPIC -I. -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
AR = ar
ARFLAGS = rcs

# Library name
LIBRARY = libdvd.a
SHARED_LIBRARY = libdvd.so

# Source files
SOURCES = dvd_disc.c \
          dvd_utils.c \
          dvd_audio.c \
          dvd_video.c \
          dvd_extract.c \
          bluray_mpls.c

OBJECTS = $(SOURCES:.c=.o)

# Headers
HEADERS = dvd_lib.h \
          dvd_internal.h

.PHONY: all clean static shared

all: static

static: $(LIBRARY)

shared: $(SHARED_LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

$(SHARED_LIBRARY): $(OBJECTS)
	$(CC) -shared -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(LIBRARY) $(SHARED_LIBRARY)

# Test program
test_dvd: test_dvd.c $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $< -L. -ldvd

# Install headers (optional)
install-headers:
	mkdir -p ../include
	cp dvd_lib.h ../include/

.PHONY: install-headers