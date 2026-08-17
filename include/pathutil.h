#ifndef UB_PATHUTIL_H
#define UB_PATHUTIL_H

#include <windows.h>

/*
 * Все пути в проекте — wide-char (UTF-16), т.к. Win32 API нативно
 * работает с ними, и это даёт нам CF_UNICODETEXT "бесплатно".
 *
 * Правило: НИКАКИХ ручных конкатенаций "%s/%s" или "%s\\%s" в коде
 * детекторов/main. Весь путь строится только через ub_path_join(),
 * который сам следит за наличием/отсутствием разделителя и всегда
 * использует '\\' (Windows-нативный разделитель). Это устраняет
 * баги вида "двойной бэкслеш", "прямой слэш вместо обратного"
 * и переполнение буфера при склейке.
 */

/* Собирает out = base + "\\" + leaf, безопасно (с учётом MAX_PATH).
 * Возвращает TRUE при успехе, FALSE при переполнении/ошибке. */
BOOL ub_path_join(WCHAR *out, size_t out_cap, const WCHAR *base, const WCHAR *leaf);

/* Возвращает %LOCALAPPDATA%\UnderBuffer в out (создаёт папку, если её нет).
 * Используется для лог-файла и (в будущем) конфига. */
BOOL ub_get_app_data_dir(WCHAR *out, size_t out_cap);

#endif /* UB_PATHUTIL_H */
