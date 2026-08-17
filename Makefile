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

# --- detector unit tests ----------------------------------------------------
# Run with native gcc (no mingw, no Windows) via tests/compat/windows.h.
# detector_url.c/detector_json.c/detector_unbreak.c/pipeline.c make zero real
# Win32 calls — just WCHAR/BOOL and a couple of CRT functions — so their logic
# can be checked far faster than building the exe and copy-pasting by hand.
# clipboard.c/main.c/netcheck.c are not covered by these tests.
TEST_CC    := gcc
TEST_SRC   := tests/test_detectors.c src/pipeline.c src/detector_url.c src/detector_json.c src/detector_unbreak.c
TEST_BIN   := build/test_detectors
TEST_FLAGS := -Wall -Wextra -std=c99 -Iinclude -Itests/compat -g

test: build
	$(TEST_CC) $(TEST_FLAGS) $(TEST_SRC) -o $(TEST_BIN)
	./$(TEST_BIN)

.PHONY: all clean test
