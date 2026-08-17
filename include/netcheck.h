#ifndef UB_NETCHECK_H
#define UB_NETCHECK_H

#include <windows.h>

/*
 * Асинхронная (в отдельном потоке) проверка доступности URL через
 * HEAD-запрос. НИКОГДА не вызывается из WM_CLIPBOARDUPDATE напрямую —
 * только через ub_netcheck_start_async(), которая мгновенно
 * возвращает управление, а сам запрос уходит в свой worker thread.
 *
 * MVP-поведение: результат проверки сейчас только логируется
 * (ub_log). Он НЕ переписывает буфер обмена повторно — это сознательно
 * отложено на следующую итерацию, чтобы не тянуть в MVP гонку
 * "пользователь скопировал что-то новое, пока мы ждали ответ сервера".
 * Задел под будущее расширение: passing clipboard_seq уже сделан,
 * так что при добавлении rewrite-логики достаточно вызвать
 * ub_clipboard_write_if_fresh() внутри воркера — без переписывания
 * остального кода.
 */
void ub_netcheck_start_async(const WCHAR *url, DWORD clipboard_seq_at_capture);

#endif /* UB_NETCHECK_H */
