#include <windows.h>
#include "clipboard.h"
#include "detector.h"
#include "netcheck.h"
#include "log.h"
#include <wchar.h>

#define WM_UB_TRAYICON (WM_APP + 1)
#define ID_TRAY_EXIT   1001

static const WCHAR *WINDOW_CLASS = L"UnderBufferListenerWnd";
static NOTIFYICONDATAW g_nid = { 0 };

static BOOL looks_like_bare_url(const WCHAR *s)
{
    while (*s == L' ' || *s == L'\t' || *s == L'\r' || *s == L'\n') s++;
    return (wcsncmp(s, L"http://", 7) == 0 || wcsncmp(s, L"https://", 8) == 0);
}

static void handle_clipboard_update(HWND hwnd)
{
    /* Инвариант anti-recursion: если это МЫ только что записали в
     * буфер, WM_CLIPBOARDUPDATE, который придёт в ответ, должен быть
     * проигнорирован целиком. */
    if (InterlockedCompareExchange(&g_is_internal_update, 0, 0) != 0) {
        return;
    }

    DWORD seq = 0;
    WCHAR *original = ub_clipboard_read_text(&seq);
    if (!original) {
        return; /* не текст, буфер занят, или пусто — пропускаем */
    }

    WCHAR *processed = ub_pipeline_run_sync(original);
    if (!processed) {
        free(original);
        return; /* OOM в пайплайне — оставляем буфер как есть */
    }

    if (wcscmp(original, processed) != 0) {
        BOOL written = ub_clipboard_write_if_fresh(hwnd, processed, seq);
        if (written) {
            ub_log(L"pipeline: rewrote clipboard (%zu -> %zu chars)",
                    wcslen(original), wcslen(processed));

            /* Асинхронная (не блокирующая) проверка доступности, если
             * итоговый текст — голый URL. Результат сейчас только
             * логируется, буфер повторно не переписывается (см. netcheck.h). */
            if (looks_like_bare_url(processed)) {
                DWORD fresh_seq = ub_clipboard_sequence();
                ub_netcheck_start_async(processed, fresh_seq);
            }
        } else {
            ub_log(L"pipeline: skipped rewrite, clipboard changed under us (stale seq)");
        }
    }

    free(original);
    free(processed);
}

static void add_tray_icon(HWND hwnd)
{
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_UB_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"UnderBuffer");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void show_tray_menu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Выход");
    SetForegroundWindow(hwnd); /* нужно, чтобы меню корректно закрывалось по клику вне */
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CLIPBOARDUPDATE:
        handle_clipboard_update(hwnd);
        return 0;

    case WM_UB_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            show_tray_menu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        RemoveClipboardFormatListener(hwnd);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    /* Однократный запуск: именованный мьютекс не даёт поднять второй
     * инстанс демона, который начал бы драться за буфер сам с собой. */
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\UnderBuffer_SingleInstance");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    if (!ub_log_init()) {
        /* Лог не критичен для работы — продолжаем без него. */
    }
    ub_log(L"UnderBuffer starting up");

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS;
    RegisterClassExW(&wc);

    /* HWND_MESSAGE: message-only окно, никогда не видно пользователю
     * и не появляется в Alt+Tab / панели задач — идеально для демона. */
    HWND hwnd = CreateWindowExW(0, WINDOW_CLASS, L"UnderBuffer",
                                 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (!hwnd) {
        ub_log(L"CreateWindowExW failed: %lu", GetLastError());
        return 1;
    }

    add_tray_icon(hwnd);

    if (!AddClipboardFormatListener(hwnd)) {
        ub_log(L"AddClipboardFormatListener failed: %lu", GetLastError());
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ub_log(L"UnderBuffer shutting down");
    ub_log_shutdown();
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    return 0;
}
