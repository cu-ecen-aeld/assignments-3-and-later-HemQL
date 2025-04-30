SRCDIR := server
SRC := $(SRCDIR)/aesdsocket.c
TARGET := $(SRCDIR)/aesdsocket

# Toolchain (can be overridden for cross-compilation)
CC := $(CROSS_COMPILE)gcc

# Compiler flags
CFLAGS := -Wall -Werror -Wextra -pedantic

# Targets
all: 
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) 

clean:
	$(RM) $(TARGET)
