#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/*
 * IMPORTANT (MVP scope): this detector only does synchronous, offline
 * normalization — stripping tracker query params. No network calls here,
 * ever. The HEAD-check for link availability lives separately in
 * netcheck.c and is only invoked from the async stage (see main.c), so
 * the WM_CLIPBOARDUPDATE handler never blocks on the network.
 */

/* Known tracker params, stripped entirely. Matched by name prefix
 * (up to '=' or end of key). */
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
            /* Either exact match, or prefix match ending in '_' (utm_*). */
            if (plen == key_len || TRACKER_PARAM_PREFIXES[i][plen - 1] == L'_') {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* Finds the first "http://" or "https://" occurrence in the string.
 * Returns NULL if there's no URL — the detector then doesn't apply. */
static const WCHAR *find_url_start(const WCHAR *s)
{
    const WCHAR *p1 = wcsstr(s, L"http://");
    const WCHAR *p2 = wcsstr(s, L"https://");
    if (p1 && p2) return (p1 < p2) ? p1 : p2;
    return p1 ? p1 : p2;
}

/* End of the URL: first whitespace/newline char, or end of string. */
static const WCHAR *find_url_end(const WCHAR *start)
{
    const WCHAR *p = start;
    while (*p && *p != L' ' && *p != L'\t' && *p != L'\r' && *p != L'\n') {
        p++;
    }

    /* Trim trailing punctuation that's almost always wrapping around the
     * URL rather than part of it: "(url)", "url.", "url,", markdown
     * "[text](url)", etc. Without this the punctuation ends up inside
     * the query string and gets mangled when a tracker param at the end
     * is stripped (the closing bracket goes with it). Peel one char at
     * a time — this also covers compound cases like "url).". */
    while (p > start) {
        WCHAR last = *(p - 1);
        if (last == L')' || last == L']' || last == L'}' || last == L'>' ||
            last == L'"' || last == L'\'' || last == L'.' || last == L',' ||
            last == L';' || last == L':' || last == L'!' || last == L'?') {
            p--;
        } else {
            break;
        }
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
        return NULL; /* no query string — nothing to strip */
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
    /*
     * Scans the whole buffer and strips trackers from EVERY http(s) URL
     * it finds, no matter what surrounds it (a bare link, a caption, a
     * markdown wrapper like "[text](url)", etc). Everything that isn't
     * part of a URL (prefix/suffix/text between links) is copied to the
     * output byte-for-byte — the detector only touches the query string
     * inside the URLs it finds, so the risk of mangling surrounding text
     * is minimal.
     *
     * strip_trackers only REMOVES characters, never adds — so the result
     * is guaranteed to be no longer than the input, and the output
     * buffer can be allocated once, no realloc needed.
     */
    size_t len = wcslen(input);
    if (len == 0) return NULL;

    WCHAR *out = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
    if (!out) return NULL;
    size_t out_len = 0;
    BOOL changed = FALSE;

    const WCHAR *cursor = input;
    while (*cursor) {
        const WCHAR *url_start = find_url_start(cursor);
        if (!url_start) {
            size_t rest = wcslen(cursor);
            memcpy(out + out_len, cursor, rest * sizeof(WCHAR));
            out_len += rest;
            break;
        }

        size_t prefix_len = (size_t)(url_start - cursor);
        memcpy(out + out_len, cursor, prefix_len * sizeof(WCHAR));
        out_len += prefix_len;

        const WCHAR *url_end = find_url_end(url_start);
        WCHAR *cleaned = strip_trackers(url_start, url_end);
        if (cleaned) {
            size_t clean_len = wcslen(cleaned);
            memcpy(out + out_len, cleaned, clean_len * sizeof(WCHAR));
            out_len += clean_len;
            free(cleaned);
            changed = TRUE;
        } else {
            size_t raw_len = (size_t)(url_end - url_start);
            memcpy(out + out_len, url_start, raw_len * sizeof(WCHAR));
            out_len += raw_len;
        }

        cursor = url_end;
    }

    out[out_len] = L'\0';

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}
