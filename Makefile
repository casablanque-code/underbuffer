CC      := x86_64-w64-mingw32-gcc
CFLAGS  := -municode -Wall -Wextra -std=c99 -Iinclude -O2
LDFLAGS := -municode -mwindows -lshell32 -lole32 -lwinhttp -luuid

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
BIN := build/underbuffer.exe

all: $(BIN)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
