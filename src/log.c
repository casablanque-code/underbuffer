#include "log.h"
#include "pathutil.h"
#include <stdio.h>
#include <stdarg.h>

static FILE *g_log_file = NULL;
static CRITICAL_SECTION g_log_lock;

BOOL ub_log_init(void)
{
    WCHAR app_dir[MAX_PATH];
    if (!ub_get_app_data_dir(app_dir, MAX_PATH)) {
        return FALSE;
    }

    /* Единственное место, где строится путь к лог-файлу — через
     * ub_path_join(), никаких литералов вида L"%s/%s" или ручных
     * L"\\\\" склеек в остальном коде. */
    WCHAR log_path[MAX_PATH];
    if (!ub_path_join(log_path, MAX_PATH, app_dir, L"underbuffer.log")) {
        return FALSE;
    }

    InitializeCriticalSection(&g_log_lock);
    g_log_file = _wfopen(log_path, L"a, ccs=UTF-8");
    return g_log_file != NULL;
}

void ub_log(const WCHAR *fmt, ...)
{
    if (!g_log_file) return;

    EnterCriticalSection(&g_log_lock);

    SYSTEMTIME st;
    GetLocalTime(&st);
    fwprintf(g_log_file, L"[%04d-%02d-%02d %02d:%02d:%02d] ",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    va_list args;
    va_start(args, fmt);
    vfwprintf(g_log_file, fmt, args);
    va_end(args);

    fwprintf(g_log_file, L"\n");
    fflush(g_log_file);

    LeaveCriticalSection(&g_log_lock);
}

void ub_log_shutdown(void)
{
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
        DeleteCriticalSection(&g_log_lock);
    }
}
