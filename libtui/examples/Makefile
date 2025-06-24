# Examples Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -I../include
LDFLAGS = -L.. -Wl,-rpath,..
LIBS = -ltui -lncurses

EXAMPLES = demo

.PHONY: all clean

all: $(EXAMPLES)

demo: demo.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	rm -f $(EXAMPLES)