# Date :- 1/23/26
# Author :- Aditya Ganesh
# Compiler selection
CC := $(CROSS_COMPILE)gcc

# Target and sources
TARGET := writer
SRC := writer.c
OBJ := $(SRC:.c=.o)

# Compiler flags
CFLAGS := -Wall -Werror

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
