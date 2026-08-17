#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/*
 * Glues text broken by newlines where it clearly isn't a real paragraph
 * break: PDF/terminal copy-paste. Conservative heuristic (better to
 * skip a break than to wreck real Markdown/code):
 *
 *  - "word-\nbreak"  -> "wordbreak"   (trailing hyphen = word wrap)
 *  - "word\nword"    -> "word word"   (single \n between words, NOT
 *                        before a list marker/blank line/capital letter
 *                        after a period — looks like a paragraph)
 *  - "word\n\nword"  left as-is — a double newline is a real paragraph
 *                        boundary, we don't touch it.
 *  - multiple/double spaces collapse into one.
 *
 * If the input has no single \n at all (already normal text/code), we
 * return NULL — detector not applied.
 */

/*
 * Deliberately NOT using iswalnum(): it depends on the process's
 * current LC_CTYPE locale, and the app never calls setlocale() (and
 * shouldn't have to — that's global process state for the sake of one
 * function). Under the default locale ("C", which is what any Win32
 * process starts with unless explicitly changed), iswalnum() returns
 * FALSE for anything outside ASCII, including Cyrillic — meaning the
 * whole line-unbreak feature would silently stop working on Russian
 * text. Explicit code ranges don't depend on locale and behave the
 * same in the exe on Windows and in the unit tests on Linux.
 */
static BOOL is_word_char(WCHAR c)
{
    if ((c >= L'0' && c <= L'9') ||
        (c >= L'a' && c <= L'z') ||
        (c >= L'A' && c <= L'Z')) {
        return TRUE;
    }
    /* Cyrillic: base block U+0400-04FF + supplement U+0500-052F. */
    if ((c >= 0x0400 && c <= 0x04FF) || (c >= 0x0500 && c <= 0x052F)) {
        return TRUE;
    }
    return FALSE;
}

WCHAR *ub_detect_unbreak(const WCHAR *input)
{
    size_t len = wcslen(input);
    if (len == 0) return NULL;

    /* Fast pre-filter: no single \n at all means nothing to do. */
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

    for (size_t i = 0; i < len; i++) {
        WCHAR c = input[i];

        if (c == L'\n') {
            BOOL prev_nl = (i > 0 && input[i - 1] == L'\n');
            BOOL next_nl = (i + 1 < len && input[i + 1] == L'\n');
            if (prev_nl || next_nl) {
                out[out_len++] = c; /* paragraph boundary — leave it alone */
                continue;
            }

            WCHAR after = (i + 1 < len) ? input[i + 1] : L'\0';

            /* Hyphenated word wrap: "word-\nbreak" -> "wordbreak" */
            WCHAR last_written = (out_len > 0) ? out[out_len - 1] : L'\0';
            if (last_written == L'-' && is_word_char(after) &&
                out_len > 1 && is_word_char(out[out_len - 2])) {
                out_len--; /* drop the hyphen itself */
                changed = TRUE;
                continue; /* don't write the \n either */
            }

            /* Looks like a list marker/new item — don't glue. */
            if (after == L'-' || after == L'*' || (after >= L'0' && after <= L'9')) {
                out[out_len++] = c;
                continue;
            }

            /* For the "should we glue?" check, look PAST any spaces
             * already written: in "word \nword" the real character
             * before the break is the letter 'd', not the space. The
             * old check looked directly at out[out_len-1] and, hitting
             * a space, fell through to "keep \n as-is" — leaking a raw
             * newline right next to the space that was already there. */
            size_t back = out_len;
            while (back > 0 && out[back - 1] == L' ') back--;
            WCHAR before_word = (back > 0) ? out[back - 1] : L'\0';
            BOOL had_trailing_space = (back != out_len);

            /* Regular "soft" line break mid-sentence -> replace with a
             * space (or just drop the \n if a space is already there —
             * two in a row can't happen, the collapsing below guarantees
             * that). */
            if (is_word_char(before_word) && (is_word_char(after) || after == L'"' || after == L'\'')) {
                if (!had_trailing_space) {
                    out[out_len++] = L' ';
                }
                changed = TRUE;
                continue;
            }

            out[out_len++] = c;
            continue;
        }

        /* Collapse runs of spaces/tabs into a single space. */
        if (c == L' ' || c == L'\t') {
            if (out_len > 0 && out[out_len - 1] == L' ') {
                changed = TRUE;
                continue;
            }
            out[out_len++] = L' ';
            continue;
        }

        out[out_len++] = c;
    }

    out[out_len] = L'\0';

    if (!changed) {
        free(out);
        return NULL;
    }
    return out;
}
