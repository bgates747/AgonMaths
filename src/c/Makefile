# Makefile for f16dec library

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -fPIC -O2
LDFLAGS = -shared -lm

# Directories
BUILD_DIR = build
LIB_DIR = lib

# Files
SRC = f16dec.c
HDR = f16dec.h
TEST_SRC = test_f16dec.c
LIB = libf16dec.so
TEST = test_f16dec

# Default target
all: directories $(LIB_DIR)/$(LIB) $(BUILD_DIR)/$(TEST)

# Create necessary directories
directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(LIB_DIR)

# Compile the shared library
$(LIB_DIR)/$(LIB): $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

# Compile the test program
$(BUILD_DIR)/$(TEST): $(TEST_SRC) $(LIB_DIR)/$(LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) -L$(LIB_DIR) -lf16dec -Wl,-rpath,$(shell pwd)/$(LIB_DIR)

# Clean generated files
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)

# Run the test program
test: $(BUILD_DIR)/$(TEST)
	$(BUILD_DIR)/$(TEST)

# Install library and header to system (optional)
install: $(LIB_DIR)/$(LIB)
	install -d $(DESTDIR)/usr/local/lib
	install -d $(DESTDIR)/usr/local/include
	install -m 644 $(HDR) $(DESTDIR)/usr/local/include
	install -m 755 $(LIB_DIR)/$(LIB) $(DESTDIR)/usr/local/lib
	ldconfig

.PHONY: all clean test directories install