#ifndef UB_CLIPBOARD_H
#define UB_CLIPBOARD_H

#include <windows.h>

/* Call at the top of the WM_CLIPBOARDUPDATE handler. Returns TRUE if
 * the current clipboard content is the result of our own most recent
 * write via ub_clipboard_write_if_fresh(), on ANY thread -- including
 * the netcheck revert, which runs on a worker thread, not the main
 * thread. Compares actual clipboard sequence numbers rather than a
 * before/after boolean flag: a flag toggled around the write on a
 * background thread can flip back to "off" before the main thread
 * even gets to dispatch the resulting WM_CLIPBOARDUPDATE, since
 * nothing guarantees ordering between "another thread finishes a
 * write" and "the main thread's message loop processes the message
 * that write triggered". Sequence numbers don't have that race: they
 * only change on real clipboard writes, and comparing them works no
 * matter which thread wrote or how delayed message dispatch is. */
BOOL ub_clipboard_is_own_update(void);

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
