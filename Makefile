# C compiler to use
CC = clang
# flags to pass to the compiler and linker
CFLAGS = -std=c99 -Wall -pedantic
LDFLAGS =

all: vyr

vyr.o: vyr.c vyr.h
	$(CC) $(CFLAGS) -c $< -o $@

cli.o: cli.c
	$(CC) $(CFLAGS) -c $< -o $@

vyr: vyr.o cli.o
	$(CC) $(LDFLAGS) $^ -o $@

# our sub-commands
.PHONY: debug clean

# debug mode
debug: CFLAGS += -D__DEBUG -g
debug: LDFLAGS += -g
debug: all

# clean the worktree
clean:
	rm -rf cli.o vyr.o vyr
