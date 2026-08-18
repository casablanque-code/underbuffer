#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/* Deliberately not iswalnum(): it's locale-dependent, and the process
 * never calls setlocale(), so under the default "C" locale it treats
 * everything non-ASCII (Cyrillic included) as not-a-letter. */
static BOOL is_word_char(WCHAR c)
{
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
        return TRUE;
    }
    if (c < 0x80) return FALSE;
    switch (c) {
    case 0x2013: case 0x2014: /* dashes */
    case 0x2018: case 0x2019: /* quotes */
    case 0x201C: case 0x201D:
    case 0x2026: /* ellipsis */
        return FALSE;
    default:
        return TRUE;
    }
}

/* A newline event is "\n" or "\r\n" -- Windows clipboard text is
 * conventionally CRLF, and the whole point of this detector breaks
 * if "\r" isn't treated as part of the line ending. Returns the
 * event length (1 or 2), or 0 if there's no newline at pos. */
static size_t newline_len(const WCHAR *s, size_t pos, size_t len)
{
    if (pos >= len) return 0;
    if (s[pos] == L'\n') return 1;
    if (s[pos] == L'\r' && pos + 1 < len && s[pos + 1] == L'\n') return 2;
    return 0;
}

WCHAR *ub_detect_unbreak(const WCHAR *input)
{
    size_t len = wcslen(input);
    if (len == 0) return NULL;

    BOOL has_newline = FALSE;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == L'\n') { has_newline = TRUE; break; }
    }
    if (!has_newline) return NULL;

    WCHAR *out = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
    if (!out) return NULL;
    size_t out_len = 0;
    BOOL changed = FALSE;

    size_t i = 0;
    while (i < len) {
        size_t nl_span = newline_len(input, i, len);

        if (nl_span > 0) {
            size_t event_end = i + nl_span;
            BOOL next_is_nl = newline_len(input, event_end, len) > 0;
            BOOL prev_is_nl = (out_len > 0 && out[out_len - 1] == L'\n');

            if (prev_is_nl || next_is_nl) {
                memcpy(out + out_len, input + i, nl_span * sizeof(WCHAR));
                out_len += nl_span;
                i = event_end;
                continue;
            }

            size_t before_idx = out_len;
            while (before_idx > 0 && out[before_idx - 1] == L' ') before_idx--;
            WCHAR before = (before_idx > 0) ? out[before_idx - 1] : L'\0';
            WCHAR after = (event_end < len) ? input[event_end] : L'\0';

            if (after == L'-' || after == L'*' || (after >= L'0' && after <= L'9')) {
                memcpy(out + out_len, input + i, nl_span * sizeof(WCHAR));
                out_len += nl_span;
                i = event_end;
                continue;
            }

            if (before == L'-' && is_word_char(after) &&
                before_idx > 1 && is_word_char(out[before_idx - 2])) {
                out_len = before_idx - 1;
                changed = TRUE;
                i = event_end;
                continue;
            }

            if (is_word_char(before) && is_word_char(after)) {
                out_len = before_idx;
                out[out_len++] = L' ';
                changed = TRUE;
                i = event_end;
                continue;
            }

            memcpy(out + out_len, input + i, nl_span * sizeof(WCHAR));
            out_len += nl_span;
            i = event_end;
            continue;
        }

        WCHAR c = input[i];
        if (c == L' ' || c == L'\t') {
            if (out_len > 0 && out[out_len - 1] == L' ') {
                changed = TRUE;
                i++;
                continue;
            }
            out[out_len++] = L' ';
            i++;
            continue;
        }

        out[out_len++] = c;
        i++;
    }

    out[out_len] = L'\0';
    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}
