#include "config_io.h"
#include "pathutil.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

static const WCHAR *DEFAULT_CONFIG_TEMPLATE =
    L"# UnderBuffer config\n"
    L"# Lines starting with # are ignored. One key=value per line.\n"
    L"# Unknown keys are ignored, so old config files stay valid across updates.\n"
    L"\n"
    L"# Background HEAD check on cleaned links (does not block the clipboard\n"
    L"# rewrite itself). If the cleaned link turns out to be broken, the\n"
    L"# original (uncleaned) clipboard text is restored automatically --\n"
    L"# some links only work with specific tracking params attached.\n"
    L"netcheck_enabled=true\n"
    L"\n"
    L"# Milliseconds before the check gives up on a link.\n"
    L"netcheck_timeout_ms=3000\n"
    L"\n"
    L"# Start UnderBuffer automatically when you log into Windows.\n"
    L"autorun_enabled=false\n"
    L"\n"
    L"# Extra query parameters to strip from links, on top of the built-in\n"
    L"# list (utm_*, fbclid, gclid, si, ...). One per line, exact match,\n"
    L"# case-insensitive.\n"
    L"# extra_tracker_param=ref\n"
    L"# extra_tracker_param=my_custom_param\n";

static BOOL write_default_config(const WCHAR *path)
{
    FILE *f = _wfopen(path, L"w, ccs=UTF-8");
    if (!f) return FALSE;
    fputws(DEFAULT_CONFIG_TEMPLATE, f);
    fclose(f);
    return TRUE;
}

static WCHAR *read_file_text(const WCHAR *path)
{
    FILE *f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return NULL; }

    /* size is a byte count from ftell(); allocate generously in
     * WCHARs, actual decoded length will be <= that. */
    size_t cap = (size_t)size + 1;
    WCHAR *buf = (WCHAR *)malloc(cap * sizeof(WCHAR));
    if (!buf) { fclose(f); return NULL; }

    size_t read = fread(buf, sizeof(WCHAR), cap - 1, f);
    buf[read] = L'\0';
    fclose(f);
    return buf;
}

void ub_config_load(ub_config_t *cfg)
{
    ub_config_defaults(cfg);

    WCHAR app_dir[MAX_PATH];
    if (!ub_get_app_data_dir(app_dir, MAX_PATH)) {
        ub_log(L"config: could not resolve app data dir, using defaults");
        return;
    }

    WCHAR config_path[MAX_PATH];
    if (!ub_path_join(config_path, MAX_PATH, app_dir, L"config.ini")) {
        ub_log(L"config: could not build config path, using defaults");
        return;
    }

    DWORD attrs = GetFileAttributesW(config_path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        if (write_default_config(config_path)) {
            ub_log(L"config: created default config at %s", config_path);
        } else {
            ub_log(L"config: could not create default config, using defaults");
        }
        return; /* defaults already in cfg, no need to re-read what we just wrote */
    }

    WCHAR *text = read_file_text(config_path);
    if (!text) {
        ub_log(L"config: could not read %s, using defaults", config_path);
        return;
    }

    ub_config_parse(text, cfg);
    free(text);
    ub_log(L"config: loaded from %s", config_path);
}
