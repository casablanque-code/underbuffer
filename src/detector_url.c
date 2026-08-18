#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/* No base64/URL-decode detector on purpose: auto-decoding arbitrary
 * strings risks exposing something copied deliberately as-is
 * (a token, a password, a key). */

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

/* Set once at startup from config; unset (NULL/0) is the default and
 * matches pre-config behavior exactly, so existing tests don't need
 * to call this. Not freed here -- caller (main.c) owns the config
 * struct's lifetime, which is the whole process. */
static const WCHAR *const *g_extra_trackers = NULL;
static size_t g_extra_tracker_count = 0;

void ub_url_set_extra_trackers(const WCHAR *const *list, size_t count)
{
    g_extra_trackers = list;
    g_extra_tracker_count = count;
}

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
    for (size_t i = 0; i < g_extra_tracker_count; i++) {
        size_t elen = wcslen(g_extra_trackers[i]);
        if (elen == key_len && _wcsnicmp(key, g_extra_trackers[i], key_len) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/* Markdown/quoting wrappers ("[text](url)", "url).") also end a URL. */
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

/* Returns malloc'd cleaned URL, or NULL if there's nothing to clean
 * (no query string, or no tracker params present). */
static WCHAR *clean_one_url(const WCHAR *url, size_t url_len)
{
    size_t qpos = url_len;
    for (size_t i = 0; i < url_len; i++) {
        if (url[i] == L'?') { qpos = i; break; }
    }
    if (qpos == url_len) return NULL;

    size_t hpos = url_len;
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
        return NULL;
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
