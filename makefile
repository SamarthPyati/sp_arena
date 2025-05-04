CC=cc
CFLAGS=-Wall -Wextra -std=c17 -g -Wno-unused-function

# Target names
TARGET=spar
EXAMPLE=example/example

# Default target builds everything
all: build example

# Build the arena allocator library
build: $(TARGET).c $(TARGET).h
	mkdir -p build
	$(CC) $(CFLAGS) -c -o build/$(TARGET).o $<

# Build the executable
$(TARGET): build $(TARGET).c build/$(TARGET).o
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

# Build and run tests
example: build example/example.c
	$(CC) $(CFLAGS) -o $(EXAMPLE) example/example.c build/$(TARGET).o
	./$(EXAMPLE)

# Clean up build artifacts
clean:
	rm -f build/$(TARGET).o $(TARGET) $(TEST)

.PHONY: all build test clean