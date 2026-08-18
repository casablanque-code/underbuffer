# Building from source

## Requirements

- WSL (Ubuntu/Debian) or Linux with a Windows cross-compiler.
- `mingw-w64` (provides `x86_64-w64-mingw32-gcc` and `-windres`).
- `make`.

```bash
sudo apt update
sudo apt install -y mingw-w64
```

## Build

```bash
git clone https://github.com/casablanque-code/underbuffer.git
cd underbuffer
make
```

Output: `build/underbuffer.exe`. Run `make clean` before rebuilding
after `git pull`/applying a patch -- cheap insurance against
accidentally running a stale binary. Object files under `build/`
aren't tracked by git but are rebuilt incrementally by make.

The icon (`resources/icon.ico`) is optional: without it `make` prints
a warning and builds with the default system icon. See
[resources/README.md](../resources/README.md) for converting an
`.svg` to `.ico`.

## Running

`underbuffer.exe` is a plain Win32 binary -- copy it to a Windows
machine and run it there (double-click or from PowerShell). It won't
run directly under WSL (no Windows GUI subsystem, no access to the
real Windows clipboard from inside WSL2 without extra tooling) --
building happens here, running and manual verification happen on
Windows.

If you don't have Windows handy, `wine build/underbuffer.exe` works
for a basic smoke test (Wine's clipboard is a separate X11
implementation, not 1:1 with the real Windows clipboard, but good
enough to check the detectors and the message loop).

## Detector tests (no mingw, no Windows)

```bash
make test
make stress
```

`make test` uses the system `gcc` and `tests/compat/windows.h` (a
shim standing in for `<windows.h>`) -- faster than a full mingw build
plus manual copy to Windows on every `src/detector_*.c` edit. Doesn't
cover `clipboard.c`/`main.c`/`netcheck.c` -- that's real Win32
integration, verified with an actual `make` build and a manual run on
Windows before release.

`make stress` runs concurrency and alloc/free-churn stress tests
against the same detector code; see the root README for details.

## Common issues

- **`x86_64-w64-mingw32-gcc: command not found`** -- `mingw-w64` isn't
  installed (see Requirements above).
- **Tests fail with `CP_UTF8 undeclared` or similar** -- some `.c`
  file in `TEST_SRC` (Makefile) is pulling in a real `<windows.h>`
  call that isn't in `tests/compat/windows.h`. Detectors
  (`detector_*.c`, `pipeline.c`) are meant to stay pure logic with no
  Win32 calls -- if one crept in, that's an architecture regression,
  not a reason to extend the shim.
- **Built it, but Windows still shows the old behavior** -- almost
  always a stale binary: `make clean && make`, and make sure you're
  copying the freshly built `build/underbuffer.exe`, not an old copy
  from somewhere else.
