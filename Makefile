CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -pedantic -g -pthread -Iinclude
LDFLAGS := -pthread
SRC := src/chash.c src/hash_table.c src/logger.c
OBJ := $(SRC:src/%.c=build/%.o)

.PHONY: all clean

all: chash

chash: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build chash hash.log
