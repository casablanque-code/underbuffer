#include "autorun.h"
#include "log.h"

#define RUN_KEY   L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_VALUE L"UnderBuffer"

BOOL ub_autorun_apply(BOOL enabled)
{
    HKEY hKey;
    LONG r = RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hKey);
    if (r != ERROR_SUCCESS) {
        ub_log(L"autorun: RegOpenKeyExW failed (err=%ld)", r);
        return FALSE;
    }

    BOOL ok;
    if (enabled) {
        WCHAR exe_path[MAX_PATH];
        DWORD len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            ub_log(L"autorun: GetModuleFileNameW failed");
            RegCloseKey(hKey);
            return FALSE;
        }

        WCHAR quoted[MAX_PATH + 2];
        wcscpy_s(quoted, ARRAYSIZE(quoted), L"\"");
        wcscat_s(quoted, ARRAYSIZE(quoted), exe_path);
        wcscat_s(quoted, ARRAYSIZE(quoted), L"\"");

        r = RegSetValueExW(hKey, RUN_VALUE, 0, REG_SZ,
                            (const BYTE *)quoted, (DWORD)((wcslen(quoted) + 1) * sizeof(WCHAR)));
        ok = (r == ERROR_SUCCESS);
        if (ok) {
            ub_log(L"autorun: enabled (%s)", quoted);
        } else {
            ub_log(L"autorun: RegSetValueExW failed (err=%ld)", r);
        }
    } else {
        r = RegDeleteValueW(hKey, RUN_VALUE);
        ok = (r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND);
        if (ok) {
            ub_log(L"autorun: disabled");
        } else {
            ub_log(L"autorun: RegDeleteValueW failed (err=%ld)", r);
        }
    }

    RegCloseKey(hKey);
    return ok;
}
