# Use cross compiler if provided (Buildroot sets CROSS_COMPILE)
CC ?= $(CROSS_COMPILE)gcc

CFLAGS ?= -Wall -Werror -g -pthread
LDFLAGS ?=

TARGET = aesdsocket

all: $(TARGET)
default: all

$(TARGET): aesdsocket.o
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.o $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean default

