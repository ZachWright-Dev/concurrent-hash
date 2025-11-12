CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -std=c11
LDFLAGS := -pthread

SRCS := chash.c \
        commands.c \
        hash_table.c \
        logger.c \
        scheduler.c

OBJS := $(SRCS:.c=.o)

TARGET := chash

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)


