#ifndef UB_TEST_WINDOWS_SHIM_H
#define UB_TEST_WINDOWS_SHIM_H
/*
 * Minimal stand-in for <windows.h>, used ONLY by the detector unit tests.
 *
 * detector_url.c / detector_json.c / detector_unbreak.c / pipeline.c are
 * pure C logic with zero Win32 API calls (see detector.h): they only need
 * the WCHAR/BOOL types and a couple of CRT functions with MSVC-style
 * names. That means they can be built with plain gcc on Linux/WSL, no
 * mingw and no real clipboard needed — a much faster dev loop.
 *
 * NOTE: this is NOT a substitute for the real build (Makefile,
 * x86_64-w64-mingw32-gcc). wchar_t is 4 bytes on Linux vs 2 bytes
 * (a UTF-16 code unit) for WCHAR on Windows. For the detector logic
 * (ASCII range: URLs, JSON syntax, whitespace, plus Cyrillic handled by
 * explicit code-point ranges) this difference doesn't affect behavior,
 * but final validation before a release still has to go through `make`
 * in WSL and a manual run of the exe — same as before.
 */
#include <wchar.h>
#include <string.h>
#include <wctype.h>
#include <stdlib.h>

typedef wchar_t WCHAR;
typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static inline WCHAR *_wcsdup(const WCHAR *s) {
    size_t n = wcslen(s) + 1;
    WCHAR *out = (WCHAR *)malloc(n * sizeof(WCHAR));
    if (!out) return NULL;
    memcpy(out, s, n * sizeof(WCHAR));
    return out;
}

static inline int _wcsnicmp(const WCHAR *a, const WCHAR *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        WCHAR ca = towlower(a[i]);
        WCHAR cb = towlower(b[i]);
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == L'\0') break;
    }
    return 0;
}

#endif /* UB_TEST_WINDOWS_SHIM_H */
