CC=cc
CFLAGS=-Wall -Wextra -std=c17 -g -Wno-unused-function

TARGET=sp_arena
EXAMPLES=example/example
BIN_DIR=bin

all: build example

build: $(TARGET).c $(TARGET).h
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -c -o $(BIN_DIR)/$(TARGET).o $<

$(TARGET): build $(TARGET).c $(BIN_DIR)/$(TARGET).o
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

example: build $(EXAMPLE).c
	$(CC) $(CFLAGS) -o $(EXAMPLE) $(EXAMPLE).c $(BIN_DIR)/$(TARGET).o
	./$(EXAMPLE)

clean:
	rm -f build/$(TARGET).o $(TARGET) $(TEST)

.PHONY: all build test clean