# ------------------------
#  Makefile for libf16print.so
# ------------------------

CC      := gcc
CFLAGS  := -Wall -O2 -fPIC          # -fPIC = position-independent code
LDFLAGS := -shared                  # create a .so

SRC     := f16print.c
OBJ     := $(SRC:.c=.o)
TARGET  := libf16print.so

# Default target
all: $(TARGET)

# Link the shared object
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile object file(s)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# House-keeping
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
