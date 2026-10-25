# Makefile for Huffman File Compressor (C11)
# Fully portable across Windows (MinGW/cmd) and POSIX (Linux/macOS)

ifeq ($(OS),Windows_NT)
    CC = gcc
    RM = del /Q /F
    RMTREE = rmdir /S /Q
    TARGET = huff.exe
    UNIT_TARGET = unit_tests.exe
    PYTHON = python
    SEP = \\
else
    CC ?= gcc
    RM = rm -f
    RMTREE = rm -rf
    TARGET = huff
    UNIT_TARGET = unit_tests
    PYTHON = python3
    SEP = /
endif

CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2 -Iinclude
DEBUG_FLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -g -DDEBUG -Iinclude

SRCDIR = src
INCDIR = include
TESTDIR = tests

SOURCES = $(SRCDIR)/utils.c \
          $(SRCDIR)/checksum.c \
          $(SRCDIR)/bitstream.c \
          $(SRCDIR)/huffman.c \
          $(SRCDIR)/file_format.c \
          $(SRCDIR)/compressor.c \
          $(SRCDIR)/decompressor.c

OBJECTS = $(SOURCES:.c=.o)
MAIN_OBJ = $(SRCDIR)/main.o

.PHONY: all debug test clean

all: $(TARGET)

$(TARGET): $(OBJECTS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

debug: CFLAGS = $(DEBUG_FLAGS)
debug: clean $(TARGET)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(UNIT_TARGET): $(OBJECTS) $(TESTDIR)/unit_tests.c
	$(CC) $(CFLAGS) $^ -o $@

test: $(TARGET) $(UNIT_TARGET)
	@echo --- Running C Unit Tests ---
	./$(UNIT_TARGET)
	@echo --- Running Integration Test Suite ---
	$(PYTHON) $(TESTDIR)/test_runner.py

clean:
ifeq ($(OS),Windows_NT)
	-cmd /c del /q /f $(TARGET) $(UNIT_TARGET) src\*.o *.tmp* tests\*.tmp* 2>NUL
else
	-$(RM) $(OBJECTS) $(MAIN_OBJ) $(TARGET) $(UNIT_TARGET) *.tmp* $(TESTDIR)/*.tmp*
endif
