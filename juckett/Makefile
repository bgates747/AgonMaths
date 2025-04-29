# Makefile

TARGET = dragon4
LIBNAME = lib$(TARGET).so

CXX = g++
CC = gcc
CFLAGS = -fPIC -O2
CXXFLAGS = -fPIC -O2
LDFLAGS = -shared

SRC_CPP = dragon4.cpp
SRC_C = dragon4_cffi_wrapper.c

OBJ_CPP = $(SRC_CPP:.cpp=.o)
OBJ_C = $(SRC_C:.c=.o)

all: $(LIBNAME)

$(LIBNAME): $(OBJ_CPP) $(OBJ_C)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_CPP) $(OBJ_C) $(LIBNAME)
