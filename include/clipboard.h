#ifndef UB_CLIPBOARD_H
#define UB_CLIPBOARD_H

#include <windows.h>

/* Set by ub_clipboard_write_if_fresh() around its own writes so the
 * resulting WM_CLIPBOARDUPDATE can be recognized and ignored. */
extern volatile LONG g_is_internal_update;

/* Reads CF_UNICODETEXT. Returns malloc'd string (caller frees), or
 * NULL if the clipboard holds no text (image, files, empty, busy).
 * *out_seq is set to GetClipboardSequenceNumber() at read time. */
WCHAR *ub_clipboard_read_text(DWORD *out_seq);

DWORD ub_clipboard_sequence(void);

/* Writes text back only if the clipboard sequence still matches
 * expected_seq (protects against a race with another app). Other
 * clipboard formats present alongside the text (e.g. an app that also
 * puts CF_BITMAP/CF_HDROP next to CF_UNICODETEXT) are preserved. */
BOOL ub_clipboard_write_if_fresh(HWND owner, const WCHAR *text, DWORD expected_seq);

#endif
