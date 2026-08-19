CC      := x86_64-w64-mingw32-gcc
RC      := x86_64-w64-mingw32-windres
CFLAGS  := -municode -Wall -Wextra -std=c99 -Iinclude -O2
LDFLAGS := -municode -mwindows -lshell32 -lole32 -lwinhttp -luuid -ladvapi32

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
BIN := build/underbuffer.exe

# Icon is optional: without resources/icon.ico the exe still builds,
# falling back to the system icon (see resources/README.md).
ICON := resources/icon.ico
ifneq ($(wildcard $(ICON)),)
    RES_OBJ := build/app_res.o
else
    RES_OBJ :=
endif

all: $(BIN)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/app_res.o: resources/app.rc $(ICON) | build
	$(RC) resources/app.rc -O coff -o $@

$(BIN): $(OBJ) $(RES_OBJ)
ifeq ($(RES_OBJ),)
	@echo "note: resources/icon.ico not found -- building without embedded icon (see resources/README.md)"
endif
	$(CC) $(OBJ) $(RES_OBJ) $(LDFLAGS) -o $@

build:
	mkdir -p build

clean:
	rm -rf build

# --- detector unit tests ----------------------------------------------------
# Run with native gcc (no mingw, no Windows) via tests/compat/windows.h.
# detector_url.c/detector_json.c/detector_unbreak.c/pipeline.c/config.c make
# zero real Win32 calls — just WCHAR/BOOL and a couple of CRT functions — so
# their logic can be checked far faster than building the exe and
# copy-pasting by hand.
# clipboard.c/main.c/netcheck.c/config_io.c/autorun.c are not covered here --
# they're real Win32 integration (registry, files, sockets), verified by an
# actual `make` build and a manual run on Windows.
TEST_CC    := gcc
TEST_SRC   := tests/test_detectors.c src/pipeline.c src/detector_url.c src/detector_json.c src/detector_unbreak.c src/config.c
TEST_BIN   := build/test_detectors
TEST_FLAGS := -Wall -Wextra -std=c99 -Iinclude -Itests/compat -g

test: build
	$(TEST_CC) $(TEST_FLAGS) $(TEST_SRC) -o $(TEST_BIN)
	./$(TEST_BIN)

STRESS_SRC := tests/stress_test.c src/pipeline.c src/detector_url.c src/detector_json.c src/detector_unbreak.c
STRESS_BIN := build/stress_test

stress: build
	$(TEST_CC) $(TEST_FLAGS) $(STRESS_SRC) -o $(STRESS_BIN) -lpthread
	./$(STRESS_BIN)

# ASan/UBSan build of the same stress binary -- catches use-after-free,
# buffer overflows, leaks, and undefined behavior at near-native speed,
# with zero extra packages to install (both are built into gcc). This
# replaced a Valgrind-based CI step: Valgrind needed `apt-get install`
# over the network on every run (a real, repeated source of CI hangs)
# and serializes threads under its own scheduler, which made the
# 16-thread stress run take minutes instead of seconds.
ASAN_BIN   := build/stress_test_asan
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all

stress-asan: build
	$(TEST_CC) $(TEST_FLAGS) $(ASAN_FLAGS) $(STRESS_SRC) -o $(ASAN_BIN) -lpthread
	./$(ASAN_BIN)

.PHONY: all clean test stress stress-asan
