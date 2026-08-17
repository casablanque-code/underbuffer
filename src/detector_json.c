#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/*
 * Минимальный, но корректный JSON pretty-printer. Не парсит в дерево
 * (не нужно — только форматирует), но уважает строки/экранирование,
 * чтобы не сломать отступы внутри "текста со скобками {и} запятыми,".
 * Если на вход пришло что-то, что не похоже на валидный JSON (парность
 * скобок/кавычек не сходится), возвращает NULL — детектор не применён.
 */

typedef struct {
    WCHAR *buf;
    size_t len;
    size_t cap;
} sb_t;

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

static BOOL sb_indent(sb_t *sb, int depth)
{
    if (!sb_ensure(sb, (size_t)depth * 2)) return FALSE;
    for (int i = 0; i < depth * 2; i++) sb->buf[sb->len++] = L' ';
    return TRUE;
}

/* Быстрая эвристическая проверка "похоже на JSON": первый непробельный
 * символ — { или [, и число открывающих/закрывающих скобок совпадает
 * вне строк. Дёшево и отсекает случайный текст со скобками. */
static BOOL looks_like_json(const WCHAR *s)
{
    while (*s == L' ' || *s == L'\t' || *s == L'\r' || *s == L'\n') s++;
    if (*s != L'{' && *s != L'[') return FALSE;

    int depth = 0;
    BOOL in_string = FALSE;
    for (const WCHAR *p = s; *p; p++) {
        if (in_string) {
            if (*p == L'\\' && p[1] != L'\0') { p++; continue; }
            if (*p == L'"') in_string = FALSE;
            continue;
        }
        if (*p == L'"') { in_string = TRUE; continue; }
        if (*p == L'{' || *p == L'[') depth++;
        else if (*p == L'}' || *p == L']') depth--;
        if (depth < 0) return FALSE;
    }
    return depth == 0 && !in_string;
}

WCHAR *ub_detect_json(const WCHAR *input)
{
    if (!looks_like_json(input)) return NULL;

    sb_t sb;
    if (!sb_init(&sb, wcslen(input) * 2 + 64)) return NULL;

    int depth = 0;
    BOOL in_string = FALSE;
    BOOL ok = TRUE;

    for (const WCHAR *p = input; *p && ok; p++) {
        if (in_string) {
            ok = sb_putc(&sb, *p);
            if (*p == L'\\' && p[1] != L'\0') {
                p++;
                ok = ok && sb_putc(&sb, *p);
                continue;
            }
            if (*p == L'"') in_string = FALSE;
            continue;
        }

        switch (*p) {
        case L'"':
            in_string = TRUE;
            ok = sb_putc(&sb, *p);
            break;
        case L' ': case L'\t': case L'\r': case L'\n':
            /* исходные пробелы вне строк отбрасываем — расставим свои */
            break;
        case L'{': case L'[': {
            WCHAR next = *(p + 1);
            ok = sb_putc(&sb, *p);
            if (next == L'}' || next == L']') {
                /* пустой объект/массив: {} / [] без переноса */
                break;
            }
            depth++;
            ok = ok && sb_putc(&sb, L'\n') && sb_indent(&sb, depth);
            break;
        }
        case L'}': case L']': {
            WCHAR prev = sb.len > 0 ? sb.buf[sb.len - 1] : L'\0';
            if (prev != L'{' && prev != L'[') {
                depth--;
                ok = sb_putc(&sb, L'\n') && sb_indent(&sb, depth);
                ok = ok && sb_putc(&sb, *p);
            } else {
                ok = sb_putc(&sb, *p);
            }
            break;
        }
        case L',':
            ok = sb_putc(&sb, *p) && sb_putc(&sb, L'\n') && sb_indent(&sb, depth);
            break;
        case L':':
            ok = sb_putc(&sb, *p) && sb_putc(&sb, L' ');
            break;
        default:
            ok = sb_putc(&sb, *p);
            break;
        }
    }

    if (!ok || !sb_putc(&sb, L'\0')) {
        free(sb.buf);
        return NULL;
    }
    return sb.buf;
}
