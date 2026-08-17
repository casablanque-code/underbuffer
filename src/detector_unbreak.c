#include "detector.h"
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

/*
 * Склеивает текст, разорванный переносами строк там, где это явно
 * не абзац: PDF/терминальный copy-paste. Консервативная эвристика
 * (лучше пропустить перенос, чем испортить настоящий Markdown/код):
 *
 *  - "word-\nbreak"  -> "wordbreak"   (дефис в конце строки = перенос слова)
 *  - "word\nword"    -> "word word"   (одиночный \n между словами, НЕ
 *                        перед маркером списка/пустой строкой/заглавной
 *                        буквой после точки — похоже на абзац)
 *  - "word\n\nword"  оставляем как есть — двойной перенос = настоящая
 *                        граница абзаца, её не трогаем.
 *  - двойные/множественные пробелы схлопываются в один.
 *
 * Если во входе нет одиночных \n вообще (уже нормальный текст/код) —
 * возвращаем NULL, детектор не применён.
 */

static BOOL is_word_char(WCHAR c)
{
    return iswalnum(c) != 0;
}

WCHAR *ub_detect_unbreak(const WCHAR *input)
{
    size_t len = wcslen(input);
    if (len == 0) return NULL;

    /* Быстрый предфильтр: если нет ни одного одиночного \n, делать нечего. */
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
                out[out_len++] = c; /* граница абзаца — не трогаем */
                continue;
            }

            WCHAR before = (out_len > 0) ? out[out_len - 1] : L'\0';
            WCHAR after = (i + 1 < len) ? input[i + 1] : L'\0';

            /* Дефис-перенос слова: "word-\nbreak" -> "wordbreak" */
            if (before == L'-' && is_word_char(after) && out_len > 1 && is_word_char(out[out_len - 2])) {
                out_len--; /* убираем сам дефис */
                changed = TRUE;
                continue; /* сам \n не пишем */
            }

            /* Похоже на маркер списка/новый пункт — не склеиваем. */
            if (after == L'-' || after == L'*' || (after >= L'0' && after <= L'9')) {
                out[out_len++] = c;
                continue;
            }

            /* Обычный "мягкий" перенос строки посреди предложения ->
             * заменяем на пробел (если перед ним не пробел). */
            if (is_word_char(before) && (is_word_char(after) || after == L'"' || after == L'\'')) {
                if (before != L' ') {
                    out[out_len++] = L' ';
                }
                changed = TRUE;
                continue;
            }

            out[out_len++] = c;
            continue;
        }

        /* Схлопывание множественных пробелов/табов в один пробел. */
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
