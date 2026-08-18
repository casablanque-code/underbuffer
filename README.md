# UnderBuffer

A Windows background daemon that listens to the clipboard and cleans
up copied text on the fly: strips tracking parameters from links,
rejoins text broken by line wraps (PDF/terminal copies), and
pretty-prints JSON.

No windows, no UI on top of the system -- runs from the tray.

## What it does

- **Links.** Strips `utm_*`, `fbclid`, `gclid`, `si`, and other
  tracking params from the query string. Works on a bare link, a link
  embedded in arbitrary text, and a link wrapped in markdown
  (`[text](link)`). Useful params (`?id=...`, `?q=...`) and
  `#fragment` are left alone.
- **Broken text.** If text copied from a PDF or a terminal is split
  by line wraps mid-sentence, glues it back into one flowing block,
  including hyphenated word wraps (`interes-\nting` -> `interesting`).
  Paragraph breaks (double newline) and breaks after a period/list
  marker are left untouched.
- **JSON.** If the buffer looks like compact JSON, formats it with
  indentation.

No network on the critical path: text processing is entirely local
and synchronous. An optional background (non-blocking) link
availability check runs after the fact -- if the cleaned link turns
out to be broken (a definitive HTTP 4xx/5xx, not just a flaky
network), the clipboard is automatically reverted to the original,
uncleaned text. Some links only work with specific tracking params
attached; this way stripping them never silently breaks the link you
meant to share. See [Configuration](#configuration) to disable this
or tune the timeout.

There's **no base64/URL-decode detector on purpose** -- too easy to
accidentally decode and expose something that was copied deliberately
as-is (a token, a password, a key).

## Examples

Before:
```
https://example.com/article?id=12345&utm_source=telegram&si=abc#section
```
After:
```
https://example.com/article?id=12345#section
```

Before:
```
This was a very interes-
ting experiment.
```
After:
```
This was a very interesting experiment.
```

## Handling non-text clipboard data

The daemon only touches the clipboard when `CF_UNICODETEXT` is
present (`IsClipboardFormatAvailable` before any read). An image,
files from Explorer, anything else with no text representation --
never read, never modified.

If the clipboard holds text alongside other formats at the same time
(e.g. Excel puts text, HTML, and an image together), those other
formats are **preserved** across the rewrite: `EmptyClipboard()`
inevitably clears everything (it's the only way to take clipboard
ownership in Win32), so before that, every other GMEM-based format is
snapshotted and restored after the cleaned text is written (see
`snapshot_other_formats` in `src/clipboard.c`). Handle-based formats
(`CF_BITMAP`, `CF_METAFILEPICT`, `CF_ENHMETAFILE`, etc.) aren't
GMEM-backed and can't be duplicated the same way, but a `CF_DIB`
sibling -- which almost every source app also provides -- survives.

## Configuration

`%LOCALAPPDATA%\UnderBuffer\config.ini`, created automatically on
first run with a commented template. Edit it and restart UnderBuffer
to apply changes (it's read once at startup, not watched live).

```ini
# Background HEAD check on cleaned links (does not block the clipboard
# rewrite itself). If the cleaned link turns out to be broken, the
# original (uncleaned) clipboard text is restored automatically --
# some links only work with specific tracking params attached.
netcheck_enabled=true

# Milliseconds before the check gives up on a link.
netcheck_timeout_ms=3000

# Start UnderBuffer automatically when you log into Windows.
autorun_enabled=false

# Extra query parameters to strip from links, on top of the built-in
# list (utm_*, fbclid, gclid, si, ...). One per line, exact match,
# case-insensitive.
# extra_tracker_param=ref
# extra_tracker_param=my_custom_param
```

Unknown keys are ignored, so old config files stay valid across
updates that add new options.

## Install

Download `underbuffer.exe` from [Releases](../../releases) and run
it -- no windows, it minimizes straight to the tray. Tray icon ->
right click -> Exit to stop it.

The exe isn't code-signed (this is a one-person open-source utility,
not commercial software), so on first run Windows SmartScreen may
show "Windows protected your PC". That's expected for unsigned
binaries -- click "More info" -> "Run anyway". See building from
source below if you'd rather not trust someone else's exe.

Auto-start on login: set `autorun_enabled=true` in
`config.ini` (see [Configuration](#configuration)) and restart --
UnderBuffer registers itself in `HKCU\...\Run` and will start with
Windows from then on. Flip it back to `false` to remove it.

## Building from source

See [docs/BUILD.md](docs/BUILD.md).

## How it's built

- `src/main.c` -- message-only window, listens for
  `WM_CLIPBOARDUPDATE`, tray icon, wires config/autorun at startup.
- `src/clipboard.c` -- clipboard read/write with race protection
  (checks `GetClipboardSequenceNumber` before rewriting) and
  anti-recursion (our own `SetClipboardData` must not trigger itself).
- `src/detector_*.c` + `src/pipeline.c` -- text processing, pure
  `WCHAR* -> WCHAR*` functions with no clipboard access -- which is
  why they can be unit-tested natively, without Windows (see below).
- `src/config.c` -- pure config-file parsing (also natively testable);
  `src/config_io.c` -- the disk I/O around it (create/read
  `config.ini`), kept separate so the parsing logic doesn't need real
  Win32 file APIs to test.
- `src/autorun.c` -- `HKCU\...\Run` registry entry, applied once at
  startup based on `autorun_enabled`.
- `src/netcheck.c` -- async HEAD check over WinHTTP on a worker
  thread, never blocks the main thread. The revert-on-broken-link
  decision is a pure function (`ub_netcheck_should_revert` in
  `netcheck.h`), unit-tested without a real network call.

## Tests

```bash
make test
```

Runs detector and config-parsing unit tests with native `gcc` (no
mingw, no Windows) -- `tests/test_detectors.c` +
`tests/compat/windows.h` (a minimal stand-in for `<windows.h>`
covering `WCHAR`/`BOOL`/`DWORD`/`HWND` and a couple of CRT functions).
Fast dev loop: edit `src/detector_*.c` or `src/config.c` -> `make
test` -> results in seconds.

This doesn't replace the final check: `clipboard.c`/`main.c`/
`netcheck.c`/`config_io.c`/`autorun.c` aren't covered by these tests
(real Win32 integration: registry, files, sockets) -- the final check
is a `make`-built exe, run by hand on Windows.

```bash
make stress
```

Concurrency run (16 threads x 5000 iterations against a shared set of
cases, output checked each time) and a churn test (200k alloc/free
cycles over random strings) -- catches races and leaks in the
detectors/pipeline before they reach Windows. CI also runs this under
Valgrind (`--leak-check=full`).

## CI

`.github/workflows/ci.yml`: every push/PR runs `make test`, `make
stress` (plus under Valgrind), then builds `underbuffer.exe` with
mingw-w64 and attaches it as a run artifact.

## Known gaps

- `clipboard.c` (the actual Win32 read/write, race protection, format
  preservation) has no automated test coverage -- only manual
  verification on real Windows. A `windows-latest` CI job exercising
  the real clipboard API is the natural next step; not done yet.
- Tracker param list and revert threshold (4xx/5xx) are still fixed
  in code, not configurable -- only the extra-tracker list and
  netcheck timing are, via `config.ini`.

## License

[MIT](LICENSE).
