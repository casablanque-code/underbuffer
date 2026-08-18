#include "config.h"
#include <stdlib.h>
#include <wchar.h>

void ub_config_defaults(ub_config_t *cfg)
{
    cfg->netcheck_enabled = TRUE;
    cfg->netcheck_timeout_ms = 3000;
    cfg->autorun_enabled = FALSE;
    cfg->extra_trackers = NULL;
    cfg->extra_tracker_count = 0;
}

static WCHAR *trim(WCHAR *s)
{
    while (*s == L' ' || *s == L'\t' || *s == L'\r') s++;
    size_t len = wcslen(s);
    while (len > 0 && (s[len - 1] == L' ' || s[len - 1] == L'\t' || s[len - 1] == L'\r')) {
        s[--len] = L'\0';
    }
    return s;
}

static BOOL parse_bool(const WCHAR *v, BOOL fallback)
{
    if (_wcsicmp(v, L"true") == 0 || _wcsicmp(v, L"1") == 0 || _wcsicmp(v, L"yes") == 0) return TRUE;
    if (_wcsicmp(v, L"false") == 0 || _wcsicmp(v, L"0") == 0 || _wcsicmp(v, L"no") == 0) return FALSE;
    return fallback;
}

static void add_extra_tracker(ub_config_t *cfg, const WCHAR *value)
{
    WCHAR **grown = (WCHAR **)realloc(cfg->extra_trackers,
                                       (cfg->extra_tracker_count + 1) * sizeof(WCHAR *));
    if (!grown) return;
    cfg->extra_trackers = grown;
    cfg->extra_trackers[cfg->extra_tracker_count] = _wcsdup(value);
    if (cfg->extra_trackers[cfg->extra_tracker_count]) {
        cfg->extra_tracker_count++;
    }
}

void ub_config_parse(const WCHAR *text, ub_config_t *cfg)
{
    size_t len = wcslen(text);
    WCHAR *copy = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
    if (!copy) return;
    memcpy(copy, text, (len + 1) * sizeof(WCHAR));

    size_t line_start = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || copy[i] == L'\n') {
            copy[i] = L'\0';
            WCHAR *trimmed = trim(copy + line_start);

            if (trimmed[0] != L'\0' && trimmed[0] != L'#') {
                WCHAR *eq = wcschr(trimmed, L'=');
                if (eq) {
                    *eq = L'\0';
                    WCHAR *key = trim(trimmed);
                    WCHAR *value = trim(eq + 1);

                    if (_wcsicmp(key, L"netcheck_enabled") == 0) {
                        cfg->netcheck_enabled = parse_bool(value, cfg->netcheck_enabled);
                    } else if (_wcsicmp(key, L"netcheck_timeout_ms") == 0) {
                        WCHAR *end = NULL;
                        unsigned long v = wcstoul(value, &end, 10);
                        if (end != value && v > 0) cfg->netcheck_timeout_ms = (DWORD)v;
                    } else if (_wcsicmp(key, L"autorun_enabled") == 0) {
                        cfg->autorun_enabled = parse_bool(value, cfg->autorun_enabled);
                    } else if (_wcsicmp(key, L"extra_tracker_param") == 0) {
                        if (value[0] != L'\0') add_extra_tracker(cfg, value);
                    }
                    /* unknown keys ignored -- forward compatible with older configs */
                }
            }

            line_start = i + 1;
        }
    }

    free(copy);
}

void ub_config_free(ub_config_t *cfg)
{
    for (size_t i = 0; i < cfg->extra_tracker_count; i++) {
        free(cfg->extra_trackers[i]);
    }
    free(cfg->extra_trackers);
    cfg->extra_trackers = NULL;
    cfg->extra_tracker_count = 0;
}
