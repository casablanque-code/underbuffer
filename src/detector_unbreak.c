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

WCHAR *ub_detect_unbreak(const WCHAR *input)
{
    size_t len = wcslen(input);
    if (len == 0) return NULL;

    BOOL has_single_newline = FALSE;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == L'\n') {
            BOOL prev_nl = (i > 0 && input[i - 1] == L'\n');
            BOOL next_nl = (i + 1 < len && input[i + 1] == L'\n');
            if (!prev_nl && !next_nl) { has_single_newline = TRUE; break; }
        }
    }
    if (!has_single_newline) return NULL;

    WCHAR *out = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
    if (!out) return NULL;
    size_t out_len = 0;
    BOOL changed = FALSE;

    size_t i = 0;
    while (i < len) {
        WCHAR c = input[i];

        if (c == L'\n') {
            BOOL prev_nl = (i > 0 && input[i - 1] == L'\n');
            BOOL next_nl = (i + 1 < len && input[i + 1] == L'\n');
            if (prev_nl || next_nl) {
                out[out_len++] = c;
                i++;
                continue;
            }

            size_t before_idx = out_len;
            while (before_idx > 0 && out[before_idx - 1] == L' ') before_idx--;
            WCHAR before = (before_idx > 0) ? out[before_idx - 1] : L'\0';
            WCHAR after = (i + 1 < len) ? input[i + 1] : L'\0';

            if (after == L'-' || after == L'*' || (after >= L'0' && after <= L'9')) {
                out[out_len++] = c;
                i++;
                continue;
            }

            if (before == L'-' && is_word_char(after) &&
                before_idx > 1 && is_word_char(out[before_idx - 2])) {
                out_len = before_idx - 1;
                changed = TRUE;
                i++;
                continue;
            }

            if (is_word_char(before) && is_word_char(after)) {
                out_len = before_idx;
                out[out_len++] = L' ';
                changed = TRUE;
                i++;
                continue;
            }

            out[out_len++] = c;
            i++;
            continue;
        }

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
