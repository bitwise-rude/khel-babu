CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = `pkg-config --cflags --libs sdl2`

SRCS =  main.c platform/desktop_env.c processor/cpu.c interrupts/interrupts.c PPU/ppu.c

OBJS = $(SRCS:.c=.o)

TARGET = khel-babu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c  $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) logging.txt

.PHONY: all clean
