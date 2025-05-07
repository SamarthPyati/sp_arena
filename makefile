CC=cc
INCLUDES=-I.
OPT=-O3
CFLAGS=-Wall -Wextra -std=c17 -g -Wno-unused-function $(INCLUDES) $(OPT)

SRC=sp_arena
BIN_DIR=bin

# EXAMPLES 
EXAMPLE_DIR=example
EXAMPLE_FILE=$(EXAMPLE_DIR)/example.c
EXAMPLE_BIN=$(EXAMPLE_DIR)/example

# TESTING 
TEST_DIR=tests
TEST_FILES:=$(wildcard $(TEST_DIR)/*.c)

all: $(BIN_DIR)/$(SRC).o $(EXAMPLE_BIN)

$(BIN_DIR): 
	mkdir -p $@

$(TEST_DIR): 
	mkdir -p $@

$(BIN_DIR)/$(SRC).o: $(SRC).c $(SRC).h | $(BIN_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXAMPLE_BIN): $(EXAMPLE_FILE) $(BIN_DIR)/$(SRC).o
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Built example binaries: $@"
	./$@

clean:
	rm -f $(BIN_DIR)/*.o $(EXAMPLE_BIN)

.PHONY: all clean