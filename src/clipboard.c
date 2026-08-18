#include "clipboard.h"
#include <stdlib.h>

volatile LONG g_is_internal_update = 0;

#define MAX_PRESERVED_FORMATS 32

typedef struct {
    UINT format;
    HANDLE data;
} preserved_fmt_t;

/* Handle-based formats (HBITMAP, HMETAFILE, ...) aren't GMEM handles
 * and can't be duplicated with GlobalAlloc/GlobalLock -- skip them.
 * Everything else on the clipboard is conventionally GMEM-backed. */
static BOOL is_handle_based_format(UINT fmt)
{
    switch (fmt) {
    case CF_BITMAP:
    case CF_METAFILEPICT:
    case CF_ENHMETAFILE:
    case CF_PALETTE:
    case CF_OWNERDISPLAY:
    case CF_PENDATA:
    case CF_LOCALE:
    case CF_DSPTEXT:
    case CF_DSPBITMAP:
    case CF_DSPMETAFILEPICT:
    case CF_DSPENHMETAFILE:
        return TRUE;
    default:
        return FALSE;
    }
}

/* Must be called with the clipboard already open, before EmptyClipboard. */
static int snapshot_other_formats(preserved_fmt_t *out, int max_out)
{
    int count = 0;
    UINT fmt = 0;

    while ((fmt = EnumClipboardFormats(fmt)) != 0 && count < max_out) {
        if (fmt == CF_UNICODETEXT || fmt == CF_TEXT || fmt == CF_OEMTEXT) continue;
        if (is_handle_based_format(fmt)) continue;

        HANDLE h = GetClipboardData(fmt);
        if (!h) continue;
        SIZE_T size = GlobalSize(h);
        if (size == 0) continue;
        void *src = GlobalLock(h);
        if (!src) continue;

        HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
        if (copy) {
            void *dst = GlobalLock(copy);
            if (dst) {
                memcpy(dst, src, size);
                GlobalUnlock(copy);
                out[count].format = fmt;
                out[count].data = copy;
                count++;
            } else {
                GlobalFree(copy);
            }
        }
        GlobalUnlock(h);
    }
    return count;
}

DWORD ub_clipboard_sequence(void)
{
    return GetClipboardSequenceNumber();
}

WCHAR *ub_clipboard_read_text(DWORD *out_seq)
{
    if (out_seq) {
        *out_seq = GetClipboardSequenceNumber();
    }
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return NULL;
    }

    BOOL opened = FALSE;
    for (int attempt = 0; attempt < 5 && !opened; attempt++) {
        opened = OpenClipboard(NULL);
        if (!opened) Sleep(10);
    }
    if (!opened) return NULL;

    WCHAR *result = NULL;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData != NULL) {
        WCHAR *src = (WCHAR *)GlobalLock(hData);
        if (src != NULL) {
            size_t len = wcslen(src);
            if (len > 0) {
                result = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
                if (result) {
                    memcpy(result, src, (len + 1) * sizeof(WCHAR));
                }
            }
            GlobalUnlock(hData);
        }
    }

    CloseClipboard();
    return result;
}

BOOL ub_clipboard_write_if_fresh(HWND owner, const WCHAR *text, DWORD expected_seq)
{
    if (!text) return FALSE;
    if (GetClipboardSequenceNumber() != expected_seq) return FALSE;

    size_t len = wcslen(text);
    size_t bytes = (len + 1) * sizeof(WCHAR);

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) return FALSE;
    WCHAR *dst = (WCHAR *)GlobalLock(hMem);
    if (!dst) {
        GlobalFree(hMem);
        return FALSE;
    }
    memcpy(dst, text, bytes);
    GlobalUnlock(hMem);

    BOOL opened = FALSE;
    for (int attempt = 0; attempt < 5 && !opened; attempt++) {
        opened = OpenClipboard(owner);
        if (!opened) Sleep(10);
    }
    if (!opened) {
        GlobalFree(hMem);
        return FALSE;
    }

    if (GetClipboardSequenceNumber() != expected_seq) {
        CloseClipboard();
        GlobalFree(hMem);
        return FALSE;
    }

    preserved_fmt_t preserved[MAX_PRESERVED_FORMATS];
    int preserved_count = snapshot_other_formats(preserved, MAX_PRESERVED_FORMATS);

    InterlockedExchange(&g_is_internal_update, 1);

    EmptyClipboard();
    BOOL ok = (SetClipboardData(CF_UNICODETEXT, hMem) != NULL);
    if (!ok) {
        GlobalFree(hMem);
    }

    for (int i = 0; i < preserved_count; i++) {
        if (!SetClipboardData(preserved[i].format, preserved[i].data)) {
            GlobalFree(preserved[i].data);
        }
    }

    CloseClipboard();
    InterlockedExchange(&g_is_internal_update, 0);

    return ok;
}
