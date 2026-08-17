#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/*
 * НАСТОЯЩАЯ ПРИЧИНА БАГА С КИРИЛЛИЦЕЙ (не "нужно исправить путь/слэши",
 * а именно классификация символов): предыдущая версия использовала
 * iswalnum(), а это locale-зависимая функция CRT. Приложение никогда не
 * вызывает setlocale(), поэтому процесс живёт в дефолтном locale "C",
 * где iswalnum() возвращает FALSE для всего, что не ASCII. Итог: для
 * кириллицы (и любого другого нелатинского текста) is_word_char()
 * всегда была FALSE, из-за чего склейка переносов молча не работала.
 *
 * Фикс: is_word_char() не зависит от locale вообще. ASCII-буквы/цифры
 * проверяются явно; всё, что выше 0x7F, по умолчанию считается
 * "буквой" (что верно для кириллицы, латиницы с диакритикой и т.д.),
 * за вычетом небольшого списка типографских разделителей (тире,
 * многоточие, "умные" кавычки), которые ведут себя как пунктуация,
 * а не как часть слова.
 */
static BOOL is_word_char(WCHAR c)
{
    if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) {
        return TRUE;
    }
    if (c < 0x80) {
        return FALSE; /* ASCII-пунктуация/пробелы -- не часть слова */
    }
    switch (c) {
    case 0x2013: case 0x2014: /* en/em dash */
    case 0x2018: case 0x2019: /* '  ' */
    case 0x201C: case 0x201D: /* "  " */
    case 0x2026: /* … */
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

    /* Результат никогда не длиннее входа: \n->' ' сохраняет длину,
     * склейка дефиса и схлопывание пробелов только укорачивают. */
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
                out[out_len++] = c; /* граница абзаца -- не трогаем */
                i++;
                continue;
            }

            /* Смотрим на "эффективный" символ перед \n, пропуская уже
             * записанные пробелы -- иначе "word \nword" оставляет и
             * пробел, и сырой \n (было баг-репортом от пользователя). */
            size_t before_idx = out_len;
            while (before_idx > 0 && out[before_idx - 1] == L' ') before_idx--;
            WCHAR before = (before_idx > 0) ? out[before_idx - 1] : L'\0';
            WCHAR after = (i + 1 < len) ? input[i + 1] : L'\0';

            /* Маркер списка/новый пункт после \n -- не склеиваем. */
            if (after == L'-' || after == L'*' || (after >= L'0' && after <= L'9')) {
                out[out_len++] = c;
                i++;
                continue;
            }

            /* Перенос слова через дефис: "exam-\nple" -> "example". */
            if (before == L'-' && is_word_char(after) &&
                before_idx > 1 && is_word_char(out[before_idx - 2])) {
                out_len = before_idx - 1; /* убираем сам дефис */
                changed = TRUE;
                i++;
                continue;
            }

            /* Обычный мягкий перенос посреди предложения -> один пробел. */
            if (is_word_char(before) && is_word_char(after)) {
                out_len = before_idx; /* срезаем уже записанные пробелы */
                out[out_len++] = L' ';
                changed = TRUE;
                i++;
                continue;
            }

            out[out_len++] = c; /* всё остальное -- оставляем как есть */
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
