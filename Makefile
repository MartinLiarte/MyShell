# ------------------------------
# MyShell Makefile
# ------------------------------

# Compiler
CC = gcc

# Compilation flags
CFLAGS = -Wall -Wextra -std=c99

# Debug flags
DEBUGFLAGS = -g -Wall -Wextra -std=c99

# Executable name
TARGET = myshell

# Source files
SRC = MyShell.c

# ------------------------------
# Default rule: build executable
# ------------------------------
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# ------------------------------
# Debug build
# ------------------------------
debug: CFLAGS += $(DEBUGFLAGS)
debug: clean $(TARGET)

# ------------------------------
# Clean up
# ------------------------------
clean:
	rm -f $(TARGET)