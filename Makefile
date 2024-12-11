CC = gcc
CFLAGS = -Wall -Wextra -I./src
LDFLAGS = -lbcm2835 -ljson-c -lpthread -lm

SRCS = src/main.c \
       src/adc.c \
       src/network.c \
       src/water_level.c \
       src/ph_sensor.c \
       src/thread_manager.c \
       src/config.c \
       src/logger.c

OBJS = $(SRCS:.c=.o)
TARGET = water_monitor

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)