#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/*
 * ВАЖНО (MVP-решение): этот детектор делает ТОЛЬКО синхронную,
 * оффлайн нормализацию — вырезает трекинговые параметры из query
 * string. Никаких сетевых запросов здесь нет и не будет: HEAD-проверка
 * доступности ссылки живёт отдельно в netcheck.c и вызывается только
 * из async-стадии (см. main.c), никогда из этого пайплайна. Так
 * WM_CLIPBOARDUPDATE обработчик никогда не блокируется на сети.
 */

/* Известные трекинговые параметры, которые вырезаем целиком.
 * Сравнение по префиксу имени параметра (до '=' или до конца). */
static const WCHAR *TRACKER_PARAM_PREFIXES[] = {
    L"utm_",       /* utm_source, utm_medium, utm_campaign, ... */
    L"fbclid",
    L"gclid",
    L"gclsrc",
    L"dclid",
    L"msclkid",
    L"si",         /* youtube/spotify share id */
    L"igshid",
    L"mc_eid",
    L"mc_cid",
    L"_hsenc",
    L"_hsmi",
    L"ref_src",
    L"ref_url",
    L"spm",
    L"vero_id",
};
static const size_t TRACKER_PARAM_COUNT =
    sizeof(TRACKER_PARAM_PREFIXES) / sizeof(TRACKER_PARAM_PREFIXES[0]);

static BOOL is_tracker_param(const WCHAR *key, size_t key_len)
{
    for (size_t i = 0; i < TRACKER_PARAM_COUNT; i++) {
        size_t plen = wcslen(TRACKER_PARAM_PREFIXES[i]);
        if (plen <= key_len && _wcsnicmp(key, TRACKER_PARAM_PREFIXES[i], plen) == 0) {
            /* либо точное совпадение, либо совпадение как префикс (utm_*) */
            if (plen == key_len || TRACKER_PARAM_PREFIXES[i][plen - 1] == L'_') {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* Находит первое вхождение "http://" или "https://" в строке.
 * Возвращает NULL, если ссылок нет — тогда детектор не применяется. */
static const WCHAR *find_url_start(const WCHAR *s)
{
    const WCHAR *p1 = wcsstr(s, L"http://");
    const WCHAR *p2 = wcsstr(s, L"https://");
    if (p1 && p2) return (p1 < p2) ? p1 : p2;
    return p1 ? p1 : p2;
}

/* Конец URL — первый пробельный/переносной символ или конец строки. */
static const WCHAR *find_url_end(const WCHAR *start)
{
    const WCHAR *p = start;
    while (*p && *p != L' ' && *p != L'\t' && *p != L'\r' && *p != L'\n') {
        p++;
    }
    return p;
}

static WCHAR *strip_trackers(const WCHAR *url, const WCHAR *url_end)
{
    const WCHAR *query = NULL;
    for (const WCHAR *p = url; p < url_end; p++) {
        if (*p == L'?') { query = p; break; }
    }
    if (!query) {
        return NULL; /* нет query string — нечего чистить */
    }

    size_t base_len = (size_t)(query - url);
    size_t out_cap = (size_t)(url_end - url) + 1;
    WCHAR *out = (WCHAR *)malloc(out_cap * sizeof(WCHAR));
    if (!out) return NULL;

    memcpy(out, url, base_len * sizeof(WCHAR));
    size_t out_len = base_len;
    BOOL wrote_query_mark = FALSE;

    const WCHAR *p = query + 1;
    while (p < url_end) {
        const WCHAR *amp = p;
        while (amp < url_end && *amp != L'&') amp++;
        const WCHAR *eq = p;
        while (eq < amp && *eq != L'=') eq++;
        size_t key_len = (size_t)(eq - p);

        if (!is_tracker_param(p, key_len)) {
            out[out_len++] = wrote_query_mark ? L'&' : L'?';
            wrote_query_mark = TRUE;
            memcpy(out + out_len, p, (size_t)(amp - p) * sizeof(WCHAR));
            out_len += (size_t)(amp - p);
        }
        p = (amp < url_end) ? amp + 1 : amp;
    }
    out[out_len] = L'\0';
    return out;
}

WCHAR *ub_detect_url(const WCHAR *input)
{
    const WCHAR *url_start = find_url_start(input);
    if (!url_start) return NULL;

    /* MVP сознательно обрабатывает только случай "буфер = ровно один URL
     * (возможно с пробелами вокруг)". Ссылки внутри произвольного текста
     * можно добавить позже отдельным, явно включаемым режимом — трогать
     * произвольный текст автоматически рискованно (см. договорённость
     * про risky heuristics). */
    const WCHAR *p = input;
    while (*p == L' ' || *p == L'\t' || *p == L'\r' || *p == L'\n') p++;
    if (p != url_start) return NULL;

    const WCHAR *url_end = find_url_end(url_start);
    const WCHAR *q = url_end;
    while (*q == L' ' || *q == L'\t' || *q == L'\r' || *q == L'\n') q++;
    if (*q != L'\0') return NULL; /* после URL есть что-то ещё — не трогаем */

    WCHAR *cleaned = strip_trackers(url_start, url_end);
    return cleaned; /* NULL, если чистить было нечего */
}
