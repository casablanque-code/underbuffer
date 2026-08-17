#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/*
 * ПОЧЕМУ WCHAR*, А НЕ char*+UTF-8 (см. историю коммитов):
 * Предыдущая версия гоняла текст через WideCharToMultiByte/
 * MultiByteToWideChar в pipeline.c и работала с char*-буферами
 * фиксированного размера (char url[2048], char current[4096]).
 * Это дало три проблемы разом:
 *   1) pipeline.c вызывал ub_detect_json (который остался WCHAR*-based)
 *      через несовместимый char*-прототип -> UB, JSON-форматирование
 *      молча ломалось при реальном запуске.
 *   2) tests/test_detectors.c и detector.h остались рассчитаны на
 *      старый WCHAR*->WCHAR* контракт -> make test не собирался.
 *   3) фиксированные буферы 2048/4096 -- риск обрезания длинных URL.
 * Решение: детекторы остаются на WCHAR* (нативный код единиц Windows,
 * никакой лишней конвертации), а Unicode-баг (см. detector_unbreak.c)
 * чинится точечно, без смены всей архитектуры на байты.
 *
 * Этот детектор сканирует ВЕСЬ буфер на предмет http(s)-ссылок (а не
 * только "буфер = ровно одна ссылка"), чистит трекинговые параметры
 * у каждой найденной ссылки и оставляет остальной текст как есть --
 * это покрывает и голую ссылку, и ссылку в тексте, и markdown-обёртку
 * вида [text](url).
 */

static const WCHAR *TRACKER_PARAM_PREFIXES[] = {
    L"utm_",
    L"fbclid",
    L"gclid",
    L"gclsrc",
    L"dclid",
    L"msclkid",
    L"si",
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
            if (plen == key_len || TRACKER_PARAM_PREFIXES[i][plen - 1] == L'_') {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* Символы, на которых обрывается ссылка: пробельные + типичные
 * "обёрточные" символы markdown/цитирования (закрывающая скобка
 * ссылки, кавычка и т.п.), чтобы "[text](url)" и "url)." не тянули
 * за собой лишнее. */
static BOOL is_url_delim(WCHAR c)
{
    switch (c) {
    case L' ': case L'\t': case L'\r': case L'\n':
    case L')': case L']': case L'>': case L'"': case L'\'':
        return TRUE;
    default:
        return FALSE;
    }
}

/* --- маленький растущий буфер, локальный для этого файла --- */
typedef struct { WCHAR *buf; size_t len; size_t cap; } sb_t;

static BOOL sb_init(sb_t *sb, size_t cap)
{
    sb->buf = (WCHAR *)malloc(cap * sizeof(WCHAR));
    if (!sb->buf) return FALSE;
    sb->len = 0;
    sb->cap = cap;
    return TRUE;
}

static BOOL sb_ensure(sb_t *sb, size_t extra)
{
    if (sb->len + extra + 1 <= sb->cap) return TRUE;
    size_t new_cap = sb->cap * 2;
    while (new_cap < sb->len + extra + 1) new_cap *= 2;
    WCHAR *nb = (WCHAR *)realloc(sb->buf, new_cap * sizeof(WCHAR));
    if (!nb) return FALSE;
    sb->buf = nb;
    sb->cap = new_cap;
    return TRUE;
}

static BOOL sb_putc(sb_t *sb, WCHAR c)
{
    if (!sb_ensure(sb, 1)) return FALSE;
    sb->buf[sb->len++] = c;
    return TRUE;
}

static BOOL sb_puts(sb_t *sb, const WCHAR *s, size_t n)
{
    if (n == 0) return TRUE;
    if (!sb_ensure(sb, n)) return FALSE;
    memcpy(sb->buf + sb->len, s, n * sizeof(WCHAR));
    sb->len += n;
    return TRUE;
}

/* Чистит один URL (url[0..url_len)). Возвращает malloc'нутую
 * очищенную версию, или NULL если чистить нечего (нет '?' вообще,
 * или все параметры остались как есть -- ни один не трекинговый). */
static WCHAR *clean_one_url(const WCHAR *url, size_t url_len)
{
    size_t qpos = url_len;
    for (size_t i = 0; i < url_len; i++) {
        if (url[i] == L'?') { qpos = i; break; }
    }
    if (qpos == url_len) return NULL; /* нет query string */

    size_t hpos = url_len; /* начало '#fragment', если есть */
    for (size_t i = qpos; i < url_len; i++) {
        if (url[i] == L'#') { hpos = i; break; }
    }

    sb_t out;
    if (!sb_init(&out, url_len + 8)) return NULL;
    if (!sb_puts(&out, url, qpos)) { free(out.buf); return NULL; }

    BOOL any_kept = FALSE;
    BOOL any_dropped = FALSE;
    size_t i = qpos + 1;
    while (i < hpos) {
        size_t start = i;
        while (i < hpos && url[i] != L'&') i++;
        size_t seg_len = i - start;

        size_t key_len = 0;
        while (key_len < seg_len && url[start + key_len] != L'=') key_len++;

        if (!is_tracker_param(url + start, key_len)) {
            if (!sb_putc(&out, any_kept ? L'&' : L'?')) { free(out.buf); return NULL; }
            if (!sb_puts(&out, url + start, seg_len)) { free(out.buf); return NULL; }
            any_kept = TRUE;
        } else {
            any_dropped = TRUE;
        }
        if (i < hpos && url[i] == L'&') i++;
    }

    if (!any_dropped) {
        free(out.buf);
        return NULL; /* все параметры полезные -- ссылку не трогаем */
    }

    if (hpos < url_len) {
        if (!sb_puts(&out, url + hpos, url_len - hpos)) { free(out.buf); return NULL; }
    }
    if (!sb_putc(&out, L'\0')) { free(out.buf); return NULL; }
    return out.buf;
}

WCHAR *ub_detect_url(const WCHAR *input)
{
    sb_t sb;
    if (!sb_init(&sb, wcslen(input) + 64)) return NULL;

    BOOL changed = FALSE;
    const WCHAR *p = input;

    for (;;) {
        const WCHAR *h1 = wcsstr(p, L"http://");
        const WCHAR *h2 = wcsstr(p, L"https://");
        const WCHAR *found = (h1 && h2) ? (h1 < h2 ? h1 : h2) : (h1 ? h1 : h2);

        if (!found) {
            if (!sb_puts(&sb, p, wcslen(p))) { free(sb.buf); return NULL; }
            break;
        }

        if (!sb_puts(&sb, p, (size_t)(found - p))) { free(sb.buf); return NULL; }

        const WCHAR *end = found;
        while (*end && !is_url_delim(*end)) end++;
        size_t ulen = (size_t)(end - found);

        WCHAR *cleaned = clean_one_url(found, ulen);
        if (cleaned) {
            BOOL ok = sb_puts(&sb, cleaned, wcslen(cleaned));
            free(cleaned);
            if (!ok) { free(sb.buf); return NULL; }
            changed = TRUE;
        } else {
            if (!sb_puts(&sb, found, ulen)) { free(sb.buf); return NULL; }
        }

        p = end;
    }

    if (!sb_putc(&sb, L'\0')) { free(sb.buf); return NULL; }

    if (!changed) {
        free(sb.buf);
        return NULL;
    }
    return sb.buf;
}
