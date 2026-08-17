#include "clipboard.h"
#include <stdlib.h>

volatile LONG g_is_internal_update = 0;

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

    /* OpenClipboard может кратковременно фейлиться, если буфер
     * держит другой процесс (например explorer.exe при drag&drop).
     * Несколько коротких попыток вместо мгновенного отказа. */
    BOOL opened = FALSE;
    for (int attempt = 0; attempt < 5 && !opened; attempt++) {
        opened = OpenClipboard(NULL);
        if (!opened) Sleep(10);
    }
    if (!opened) {
        return NULL;
    }

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

    /* Первая, самая частая проверка свежести — без открытия буфера. */
    if (GetClipboardSequenceNumber() != expected_seq) {
        return FALSE;
    }

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

    /* Повторная проверка ВНУТРИ критической секции OpenClipboard —
     * между первой проверкой и получением лока буфер мог измениться. */
    if (GetClipboardSequenceNumber() != expected_seq) {
        CloseClipboard();
        GlobalFree(hMem);
        return FALSE;
    }

    InterlockedExchange(&g_is_internal_update, 1);

    EmptyClipboard();
    BOOL ok = (SetClipboardData(CF_UNICODETEXT, hMem) != NULL);
    if (!ok) {
        GlobalFree(hMem); /* только при неудаче — при успехе владение переходит системе */
    }

    CloseClipboard();

    /* Флаг снимается ПОСЛЕ CloseClipboard: WM_CLIPBOARDUPDATE придёт
     * асинхронно в очередь сообщений этого же потока, так что к моменту
     * его обработки флаг уже будет виден с правильным значением. */
    InterlockedExchange(&g_is_internal_update, 0);

    return ok;
}
