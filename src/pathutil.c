#include "pathutil.h"
#include <shlobj.h>
#include <wchar.h>

BOOL ub_path_join(WCHAR *out, size_t out_cap, const WCHAR *base, const WCHAR *leaf)
{
    if (!out || !base || !leaf || out_cap == 0) {
        return FALSE;
    }

    size_t base_len = wcslen(base);
    size_t leaf_len = wcslen(leaf);

    /* Срезаем хвостовые разделители у base (любые: '\\' или '/'),
     * чтобы не получить "C:\Foo\\\\bar" при склейке. */
    while (base_len > 0 && (base[base_len - 1] == L'\\' || base[base_len - 1] == L'/')) {
        base_len--;
    }
    /* Срезаем ведущие разделители у leaf по той же причине. */
    size_t leaf_start = 0;
    while (leaf_start < leaf_len && (leaf[leaf_start] == L'\\' || leaf[leaf_start] == L'/')) {
        leaf_start++;
    }
    size_t leaf_effective_len = leaf_len - leaf_start;

    /* base_len + '\\' + leaf_effective_len + '\0' */
    size_t needed = base_len + 1 + leaf_effective_len + 1;
    if (needed > out_cap) {
        return FALSE; /* явный отказ вместо тихого усечения */
    }

    memcpy(out, base, base_len * sizeof(WCHAR));
    out[base_len] = L'\\';
    memcpy(out + base_len + 1, leaf + leaf_start, leaf_effective_len * sizeof(WCHAR));
    out[base_len + 1 + leaf_effective_len] = L'\0';

    return TRUE;
}

BOOL ub_get_app_data_dir(WCHAR *out, size_t out_cap)
{
    WCHAR *local_appdata = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &local_appdata);
    if (FAILED(hr) || local_appdata == NULL) {
        if (local_appdata) CoTaskMemFree(local_appdata);
        return FALSE;
    }

    WCHAR app_dir[MAX_PATH];
    BOOL joined = ub_path_join(app_dir, MAX_PATH, local_appdata, L"UnderBuffer");
    CoTaskMemFree(local_appdata);
    if (!joined) {
        return FALSE;
    }

    if (!CreateDirectoryW(app_dir, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            return FALSE;
        }
    }

    size_t len = wcslen(app_dir);
    if (len + 1 > out_cap) {
        return FALSE;
    }
    memcpy(out, app_dir, (len + 1) * sizeof(WCHAR));
    return TRUE;
}
