CC = gcc
CFLAGS = -Wall -Wextra -g
SRC_DIR = src
INCLUDE_DIR = include
BIN_DIR = bin

# Create bin directory if it doesn't exist
$(shell mkdir -p $(BIN_DIR))

# Source files
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/fat32.c $(SRC_DIR)/lexer.c $(SRC_DIR)/navigation.c $(SRC_DIR)/create.c
OBJS = $(SRCS:.c=.o)

# Target executable
TARGET = $(BIN_DIR)/filesys

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
