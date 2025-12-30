CC = gcc
CFLAGS = -Wall -Wextra -g

SRCS =  main.c platform/desktop_env.c processor/cpu.c PPU/ppu.c

OBJS = $(SRCS:.c=.o)

TARGET = khel-babu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c  $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) logging.txt

.PHONY: all clean
